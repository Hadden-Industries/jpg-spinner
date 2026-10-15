// Command artifact-sbom inventories explicit payload and resolved-component
// directories through Syft's public APIs. It neither extracts nor signs MSIX
// packages, and it is not an application runtime dependency.
package main

import (
	"context"
	"crypto"
	"flag"
	"fmt"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"strings"
	"time"

	nativesync "github.com/anchore/go-sync"
	nativefile "github.com/anchore/stereoscope/pkg/file"
	"github.com/anchore/syft/syft/artifact"
	"github.com/anchore/syft/syft/cataloging"
	"github.com/anchore/syft/syft/file/cataloger/filedigest"
	"github.com/anchore/syft/syft/file/cataloger/filemetadata"
	"github.com/anchore/syft/syft/format/spdxjson"
	"github.com/anchore/syft/syft/pkg"
	"github.com/anchore/syft/syft/pkg/cataloger/dotnet"
	"github.com/anchore/syft/syft/sbom"
	"github.com/anchore/syft/syft/source"
	"github.com/anchore/syft/syft/source/directorysource"
)

// inventoryArtifact composes the artifact model before native serialization. Syft
// owns cataloging, SPDX decoding, package identity and output encoding. This
// adapter never parses package manifests or edits generated JSON. Only files
// under payloadRoot become artifact files; component evidence stays separate.
func inventoryArtifact(ctx context.Context, payloadRoot, componentRoot string, identity source.Alias) (*sbom.SBOM, error) {
	if identity.Name == "" || identity.Version == "" {
		return nil, fmt.Errorf("explicit public artifact name and version are required")
	}
	// Native aliases establish identity before serialization. Never scrub an
	// absolute profile path out of generated JSON after it has already escaped.
	ctx = nativesync.SetContextExecutor(ctx, cataloging.ExecutorFile, nativesync.NewExecutor(2))
	payloadSource, err := directorysource.New(directorysource.Config{Path: payloadRoot, Base: payloadRoot, Alias: identity})
	if err != nil {
		return nil, err
	}
	defer payloadSource.Close()
	payloadResolver, err := payloadSource.FileResolver(source.SquashedScope)
	if err != nil {
		return nil, err
	}
	metadata, err := filemetadata.NewCataloger().Catalog(ctx, payloadResolver)
	if err != nil {
		return nil, err
	}
	digests, err := filedigest.NewCataloger([]crypto.Hash{crypto.SHA256}).Catalog(ctx, payloadResolver)
	if err != nil {
		return nil, err
	}
	// Direct public catalogers avoid registering irrelevant container, registry
	// and cloud providers. Syft still owns resolution, digests and encoding.
	inventory := &sbom.SBOM{
		Source:     payloadSource.Describe(),
		Descriptor: sbom.Descriptor{Name: "JPG Spinner artifact SBOM tool", Version: "development"},
		Artifacts:  sbom.Artifacts{FileMetadata: metadata, FileDigests: digests},
	}
	// Directories have no content digest and are not shipped payload files.
	// Refuse other native file kinds rather than inventing hashes for them.
	for coordinates, metadata := range inventory.Artifacts.FileMetadata {
		if metadata.Type == nativefile.TypeDirectory {
			delete(inventory.Artifacts.FileMetadata, coordinates)
		} else if metadata.Type != nativefile.TypeRegular {
			return nil, fmt.Errorf("unsupported payload file kind at %q", coordinates.RealPath)
		}
	}
	if len(inventory.Artifacts.FileMetadata) == 0 {
		return nil, fmt.Errorf("payload contains no regular files")
	}

	componentSource, err := directorysource.New(directorysource.Config{Path: componentRoot, Base: componentRoot})
	if err != nil {
		return nil, err
	}
	defer componentSource.Close()
	// NuGet's resolved lock graph is a native input. Do not re-resolve vcpkg
	// manifests: overlay ports/features belong to the actual installed build,
	// whose unmodified native SPDX documents are decoded below instead.
	componentResolver, err := componentSource.FileResolver(source.SquashedScope)
	if err != nil {
		return nil, err
	}
	packages, relationships, err := dotnet.NewDotnetPackagesLockCataloger().Catalog(ctx, componentResolver)
	if err != nil {
		return nil, err
	}
	if len(packages) == 0 {
		return nil, fmt.Errorf("component evidence contains no resolved NuGet packages")
	}
	inventory.Artifacts.Packages = pkg.NewCollection(packages...)
	installedDocumentCount := 0
	err = filepath.WalkDir(componentRoot, func(path string, entry fs.DirEntry, walkError error) error {
		if walkError != nil {
			return walkError
		}
		if err := ctx.Err(); err != nil {
			return err
		}
		if entry.Type()&fs.ModeSymlink != 0 {
			return fmt.Errorf("component evidence contains a symbolic link: %q", path)
		}
		if entry.IsDir() || entry.Name() != "vcpkg.spdx.json" {
			return nil
		}
		stream, err := os.Open(path)
		if err != nil {
			return err
		}
		decoded, formatID, version, decodeError := spdxjson.NewFormatDecoder().Decode(stream)
		closeError := stream.Close()
		if decodeError != nil {
			return decodeError
		}
		if closeError != nil {
			return closeError
		}
		if formatID != spdxjson.ID || version != "2.3" || decoded == nil {
			return fmt.Errorf("installed vcpkg evidence is not native SPDX JSON 2.3: %q", path)
		}
		if decoded.Artifacts.Packages == nil || decoded.Artifacts.Packages.PackageCount() == 0 {
			return fmt.Errorf("installed vcpkg evidence contains no packages: %q", path)
		}
		installedDocumentCount++
		// SPDX element IDs are local to each input document; vcpkg reuses
		// "port" and "binary". Establish native content-addressed package
		// identity before combining documents, and bind edges to those exact
		// new objects. Do not concatenate document-local IDs or repair JSON.
		imported := map[artifact.ID]pkg.Package{}
		for _, component := range decoded.Artifacts.Packages.Sorted() {
			originalID := component.ID()
			contentID, err := artifact.IDByHash(&component)
			if err != nil {
				return fmt.Errorf("native package identity: %w", err)
			}
			component.OverrideID(contentID)
			imported[originalID] = component
			inventory.Artifacts.Packages.Add(component)
		}
		for _, relationship := range decoded.Relationships {
			if relationship.From == nil || relationship.To == nil {
				return fmt.Errorf("native SPDX relationship has a missing endpoint")
			}
			from, fromPackage := imported[relationship.From.ID()]
			to, toPackage := imported[relationship.To.ID()]
			if fromPackage && toPackage {
				relationship.From = from
				relationship.To = to
				relationships = append(relationships, relationship)
			}
		}
		return nil
	})
	if err != nil {
		return nil, err
	}
	if installedDocumentCount == 0 {
		return nil, fmt.Errorf("component evidence contains no installed vcpkg SPDX documents")
	}
	// Keep genuine component-to-component edges. Source metadata locations
	// explain discovery, not artifact containment, so do not promote their
	// file endpoints into the shipped file inventory. Native encoding supplies
	// the document/SBOM membership relationships from this typed model.
	for _, relationship := range relationships {
		if relationship.From != nil && relationship.To != nil &&
			inventory.Artifacts.Packages.Package(relationship.From.ID()) != nil &&
			inventory.Artifacts.Packages.Package(relationship.To.ID()) != nil {
			inventory.Relationships = append(inventory.Relationships, relationship)
		}
	}
	return inventory, nil
}

// run writes one fresh native SPDX document, or returns an actionable error.
// The caller owns input extraction/verification, output validation and promotion;
// a failed write may leave diagnostic partial output and is never a valid SBOM.
func run(arguments []string, diagnostics io.Writer) error {
	options := flag.NewFlagSet("artifact-sbom", flag.ContinueOnError)
	options.SetOutput(diagnostics)
	payload := options.String("payload", "", "Explicit read-only unpacked payload root")
	components := options.String("components", "", "Explicit resolved component-evidence root")
	output := options.String("output", "", "Fresh external SPDX output file")
	name := options.String("name", "", "Public artifact name, never an absolute scan path")
	version := options.String("version", "", "Observed artifact version")
	if err := options.Parse(arguments); err != nil {
		return err
	}
	if options.NArg() != 0 {
		return fmt.Errorf("unexpected positional arguments")
	}
	if *payload == "" || *components == "" || *output == "" {
		return fmt.Errorf("payload, components and output paths are required")
	}
	if err := requireExternalOutput(*output, *payload, *components); err != nil {
		return err
	}
	ctx, cancel := context.WithTimeout(context.Background(), 60*time.Second)
	defer cancel()
	inventory, err := inventoryArtifact(ctx, *payload, *components, source.Alias{Name: *name, Version: *version})
	if err != nil {
		return err
	}
	encoder, err := spdxjson.NewFormatEncoderWithConfig(spdxjson.EncoderConfig{Version: "3.0", Pretty: true})
	if err != nil {
		return err
	}
	stream, err := os.OpenFile(*output, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0600)
	if err != nil {
		return err
	}
	encodeError := encoder.Encode(stream, *inventory)
	closeError := stream.Close()
	if encodeError != nil {
		return encodeError
	}
	if closeError != nil {
		return closeError
	}
	return nil
}

// requireExternalOutput uses the platform's path/link resolution, not a path
// prefix grammar. Output parents and both trusted inspection roots must exist.
// Extraction must separately reject untrusted package paths and reparse points.
func requireExternalOutput(outputPath string, inputRoots ...string) error {
	parent, err := filepath.EvalSymlinks(filepath.Dir(outputPath))
	if err != nil {
		return err
	}
	parent, err = filepath.Abs(parent)
	if err != nil {
		return err
	}
	for _, inputRoot := range inputRoots {
		root, err := filepath.EvalSymlinks(inputRoot)
		if err != nil {
			return err
		}
		root, err = filepath.Abs(root)
		if err != nil {
			return err
		}
		if !strings.EqualFold(filepath.VolumeName(root), filepath.VolumeName(parent)) {
			continue
		}
		relative, err := filepath.Rel(root, parent)
		if err != nil {
			return err
		}
		if filepath.IsLocal(relative) {
			return fmt.Errorf("output must be outside inventory inputs")
		}
	}
	return nil
}

// main translates the command result into the process contract used by scripts.
func main() {
	if err := run(os.Args[1:], os.Stderr); err != nil {
		fmt.Fprintln(os.Stderr, "artifact-sbom:", err)
		os.Exit(1)
	}
}
