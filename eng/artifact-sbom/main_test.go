package main

import (
	"bytes"
	"context"
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"github.com/anchore/syft/syft/format/spdxjson"
	"github.com/anchore/syft/syft/source"
)

// An empty discovery root must not become a seemingly valid dependency-free
// application SBOM. These inputs exist, but contain no resolved components.
func TestInventoryRefusesMissingComponentEvidence(t *testing.T) {
	_, err := inventoryArtifact(context.Background(), filepath.Join(contractFixtureRoot(t), "payload"), t.TempDir(), source.Alias{Name: "JpgSpinner.Contract", Version: "1.0.0"})
	if err == nil || !strings.Contains(err.Error(), "resolved NuGet packages") {
		t.Fatalf("Missing resolved component evidence must fail closed; got %v", err)
	}
}

// A NuGet lock alone cannot describe this application's native dependencies.
// Keep the lock parser real while removing only installed vcpkg evidence.
func TestInventoryRefusesMissingInstalledVcpkgEvidence(t *testing.T) {
	components := t.TempDir()
	lock, err := os.ReadFile(filepath.Join(contractFixtureRoot(t), "source", "packages.lock.json"))
	if err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(components, "packages.lock.json"), lock, 0600); err != nil {
		t.Fatal(err)
	}
	_, err = inventoryArtifact(context.Background(), filepath.Join(contractFixtureRoot(t), "payload"), components, source.Alias{Name: "JpgSpinner.Contract", Version: "1.0.0"})
	if err == nil || !strings.Contains(err.Error(), "installed vcpkg") {
		t.Fatalf("Missing installed native dependencies must fail closed; got %v", err)
	}
}

// Generated output belongs outside every inventory input, otherwise a later
// scan can silently inventory an earlier SBOM as shipped application content.
func TestCommandRefusesOutputInsidePayload(t *testing.T) {
	payload := t.TempDir()
	if err := os.WriteFile(filepath.Join(payload, "fixture.txt"), []byte("SBOM contract fixture.\n"), 0600); err != nil {
		t.Fatal(err)
	}
	output := filepath.Join(payload, "manifest.spdx.json")
	err := run([]string{"-payload", payload, "-components", filepath.Join(contractFixtureRoot(t), "source"), "-output", output, "-name", "JpgSpinner.Contract", "-version", "1.0.0"}, &bytes.Buffer{})
	if err == nil || !strings.Contains(err.Error(), "outside inventory inputs") {
		t.Fatalf("Nested output must be refused before writing; got %v", err)
	}
	if _, err := os.Stat(output); !os.IsNotExist(err) {
		t.Fatalf("Refusal left output in the payload: %v", err)
	}
}

// A valid package graph does not compensate for scanning the wrong empty drop.
func TestInventoryRefusesEmptyPayload(t *testing.T) {
	_, err := inventoryArtifact(context.Background(), t.TempDir(), filepath.Join(contractFixtureRoot(t), "source"), source.Alias{Name: "JpgSpinner.Contract", Version: "1.0.0"})
	if err == nil || !strings.Contains(err.Error(), "payload contains no regular files") {
		t.Fatalf("Empty payload must fail closed; got %v", err)
	}
}

// Native exclusive creation must preserve an existing report byte-for-byte.
func TestCommandPreservesExistingOutput(t *testing.T) {
	output := filepath.Join(t.TempDir(), "manifest.spdx.json")
	sentinel := []byte("existing report must survive\n")
	if err := os.WriteFile(output, sentinel, 0600); err != nil {
		t.Fatal(err)
	}
	err := run([]string{"-payload", filepath.Join(contractFixtureRoot(t), "payload"), "-components", filepath.Join(contractFixtureRoot(t), "source"), "-output", output, "-name", "JpgSpinner.Contract", "-version", "1.0.0"}, &bytes.Buffer{})
	if !os.IsExist(err) {
		t.Fatalf("Existing output must be refused: %v", err)
	}
	actual, err := os.ReadFile(output)
	if err != nil || !bytes.Equal(actual, sentinel) {
		t.Fatalf("Existing output was changed: %q, %v", actual, err)
	}
}

// Component evidence is not a destination either. Work in a native copied
// fixture so even a regression cannot write into the maintained source tree.
func TestCommandRefusesOutputInsideComponentEvidence(t *testing.T) {
	components := t.TempDir()
	if err := os.CopyFS(components, os.DirFS(filepath.Join(contractFixtureRoot(t), "source"))); err != nil {
		t.Fatal(err)
	}
	output := filepath.Join(components, "manifest.spdx.json")
	err := run([]string{"-payload", filepath.Join(contractFixtureRoot(t), "payload"), "-components", components, "-output", output, "-name", "JpgSpinner.Contract", "-version", "1.0.0"}, &bytes.Buffer{})
	if err == nil || !strings.Contains(err.Error(), "outside inventory inputs") {
		t.Fatalf("Nested component output must be refused: %v", err)
	}
	if _, err := os.Stat(output); !os.IsNotExist(err) {
		t.Fatalf("Refusal left output in component evidence: %v", err)
	}
}

// These literal identities come from the reviewed fixture, not the inventory
// implementation. The payload contains one regular file, not the component
// manifests or directories. A passing serializer alone cannot meet this test.
func TestRetainedPayloadAndComponentInventory(t *testing.T) {
	fixture := contractFixtureRoot(t)
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	inventory, err := inventoryArtifact(ctx, filepath.Join(fixture, "payload"), filepath.Join(fixture, "source"), source.Alias{Name: "JpgSpinner.SbomContractFixture", Version: "1.0.0"})
	if err != nil {
		t.Fatal(err)
	}
	if len(inventory.Artifacts.FileDigests) != 1 {
		t.Fatalf("Payload digest entries: got %d, want 1", len(inventory.Artifacts.FileDigests))
	}
	for coordinates, hashes := range inventory.Artifacts.FileDigests {
		if coordinates.RealPath != "/fixture.txt" {
			t.Errorf("Unexpected payload path %q", coordinates.RealPath)
		}
		if len(hashes) != 1 || hashes[0].Algorithm != "sha256" || hashes[0].Value != "634073ab8f2a89bf749373045683c28da783014554321b2de4ea222f3ac23cce" {
			t.Errorf("Payload SHA-256 differs: %+v", hashes)
		}
	}
	found := map[string]bool{}
	for _, component := range inventory.Artifacts.Packages.Sorted() {
		found[component.Name+"@"+component.Version] = true
	}
	for _, identity := range []string{"Microsoft.Windows.CppWinRT@3.0.260818.1", "libjpeg-turbo@3.2.0#1"} {
		if !found[identity] {
			t.Errorf("Missing native component identity %q", identity)
		}
	}
}

// A public artifact identity must not default to a developer's absolute scan
// path. Exercise the actual native serializer rather than a string scrubber.
func TestSerializedDocumentHasPublicArtifactIdentity(t *testing.T) {
	fixture := contractFixtureRoot(t)
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	inventory, err := inventoryArtifact(ctx, filepath.Join(fixture, "payload"), filepath.Join(fixture, "source"), source.Alias{Name: "JpgSpinner.SbomContractFixture", Version: "1.0.0"})
	if err != nil {
		t.Fatal(err)
	}
	encoder, err := spdxjson.NewFormatEncoderWithConfig(spdxjson.EncoderConfig{Version: "3.0"})
	if err != nil {
		t.Fatal(err)
	}
	var output bytes.Buffer
	if err := encoder.Encode(&output, *inventory); err != nil {
		t.Fatal(err)
	}
	var document struct {
		Graph []struct {
			Type string `json:"type"`
			Name string `json:"name"`
		} `json:"@graph"`
	}
	if err := json.Unmarshal(output.Bytes(), &document); err != nil {
		t.Fatal(err)
	}
	found := false
	for _, node := range document.Graph {
		if node.Type != "SpdxDocument" {
			continue
		}
		found = true
		if node.Name != "JpgSpinner.SbomContractFixture" {
			t.Errorf("Public document name: got %q, want the explicit artifact identity", node.Name)
		}
	}
	if !found {
		t.Fatal("Native encoder omitted the SPDX document")
	}
}

// Native vcpkg SPDX IDs are document-scoped (each port uses SPDXRef-port).
// All independently pinned overlay/runtime identities must survive composition.
func TestInstalledOverlayComponentsSurviveComposition(t *testing.T) {
	components := filepath.Join(contractFixtureRoot(t), "source")
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	inventory, err := inventoryArtifact(ctx, filepath.Join(contractFixtureRoot(t), "payload"), components, source.Alias{Name: "JpgSpinner.OverlayQualification", Version: "2.0.0"})
	if err != nil {
		t.Fatal(err)
	}
	found := map[string]bool{}
	for _, component := range inventory.Artifacts.Packages.Sorted() {
		found[component.Name+"@"+component.Version] = true
	}
	for _, identity := range []string{"libjpeg-turbo@3.2.0#1", "exiv2@0.28.9#1", "adobe-xmp-core@2025.03#1"} {
		if !found[identity] {
			t.Errorf("Actual independently pinned component was lost: %s", identity)
		}
	}
	if t.Failed() {
		return
	}
	if output := os.Getenv("JPG_SPINNER_SBOM_QUALIFICATION_OUTPUT"); output != "" {
		err := run([]string{"-payload", filepath.Join(contractFixtureRoot(t), "payload"), "-components", components, "-output", output, "-name", "JpgSpinner.OverlayQualification", "-version", "2.0.0"}, &bytes.Buffer{})
		if err != nil {
			t.Fatal(err)
		}
		t.Logf("Unmodified native serializer output: %s", output)
	}
}

// contractFixtureRoot resolves maintained synthetic inputs, never a user folder.
func contractFixtureRoot(t *testing.T) string {
	t.Helper()
	root, err := filepath.Abs(filepath.Join("..", "..", "tests", "TestData", "SbomContractFixture"))
	if err != nil {
		t.Fatal(err)
	}
	return root
}
