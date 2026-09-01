#define NOMINMAX

#include "CoefficientDigest.h"

#include <Windows.h>
#include <bcrypt.h>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <jpeglib.h>

#include <array>
#include <csetjmp>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string_view>

#pragma comment(lib, "bcrypt.lib")

namespace jpg_spinner::test_support
{
    namespace
    {
        constexpr std::size_t sha256DigestByteCount = 32;
        constexpr std::size_t hashBufferByteCount = 4096;

        // jmp_buf carries an implementation-required alignment that can add
        // harmless tail padding to this private interop structure.
#pragma warning(push)
#pragma warning(disable : 4324)
        struct JpegErrorManager final
        {
            jpeg_error_mgr base{};
            std::jmp_buf jumpBuffer{};
            std::array<char, JMSG_LENGTH_MAX> message{};
        };
#pragma warning(pop)

        struct Sha256Stream final
        {
            BCRYPT_ALG_HANDLE algorithm{nullptr};
            BCRYPT_HASH_HANDLE hash{nullptr};
            unsigned char* hashObject{nullptr};
            std::array<unsigned char, hashBufferByteCount> bufferedBytes{};
            ULONG bufferedByteCount{0};
        };

        [[noreturn]] void handleJpegError(j_common_ptr codec)
        {
            auto* const errorManager = reinterpret_cast<JpegErrorManager*>(codec->err);
            (*codec->err->format_message)(codec, errorManager->message.data());
            std::longjmp(errorManager->jumpBuffer, 1);
        }

        void copyFailureMessage(
            std::array<char, JMSG_LENGTH_MAX>& destination,
            const char* source) noexcept
        {
            static_cast<void>(strncpy_s(
                destination.data(),
                destination.size(),
                source,
                _TRUNCATE));
        }

        void destroySha256Stream(Sha256Stream& stream) noexcept
        {
            if (stream.hash != nullptr)
            {
                static_cast<void>(BCryptDestroyHash(stream.hash));
                stream.hash = nullptr;
            }
            if (stream.algorithm != nullptr)
            {
                static_cast<void>(BCryptCloseAlgorithmProvider(stream.algorithm, 0));
                stream.algorithm = nullptr;
            }
            if (stream.hashObject != nullptr)
            {
                std::free(stream.hashObject);
                stream.hashObject = nullptr;
            }
            stream.bufferedByteCount = 0;
        }

        bool initializeSha256Stream(
            Sha256Stream& stream,
            std::array<char, JMSG_LENGTH_MAX>& failureMessage) noexcept
        {
            if (BCryptOpenAlgorithmProvider(
                    &stream.algorithm,
                    BCRYPT_SHA256_ALGORITHM,
                    nullptr,
                    0) < 0)
            {
                copyFailureMessage(failureMessage, "BCrypt could not open the SHA-256 provider.");
                return false;
            }

            ULONG hashObjectByteCount = 0;
            ULONG propertyByteCount = 0;
            if (BCryptGetProperty(
                    stream.algorithm,
                    BCRYPT_OBJECT_LENGTH,
                    reinterpret_cast<PUCHAR>(&hashObjectByteCount),
                    sizeof(hashObjectByteCount),
                    &propertyByteCount,
                    0) < 0 ||
                propertyByteCount != sizeof(hashObjectByteCount) ||
                hashObjectByteCount == 0)
            {
                copyFailureMessage(
                    failureMessage,
                    "BCrypt did not provide a valid SHA-256 hash-object size.");
                destroySha256Stream(stream);
                return false;
            }

            stream.hashObject =
                static_cast<unsigned char*>(std::malloc(hashObjectByteCount));
            if (stream.hashObject == nullptr)
            {
                copyFailureMessage(
                    failureMessage,
                    "Could not allocate the BCrypt SHA-256 hash object.");
                destroySha256Stream(stream);
                return false;
            }

            if (BCryptCreateHash(
                    stream.algorithm,
                    &stream.hash,
                    stream.hashObject,
                    hashObjectByteCount,
                    nullptr,
                    0,
                    0) < 0)
            {
                copyFailureMessage(failureMessage, "BCrypt could not create the SHA-256 hash.");
                destroySha256Stream(stream);
                return false;
            }
            return true;
        }

        bool flushSha256Stream(
            Sha256Stream& stream,
            std::array<char, JMSG_LENGTH_MAX>& failureMessage) noexcept
        {
            if (stream.bufferedByteCount == 0)
            {
                return true;
            }
            if (BCryptHashData(
                    stream.hash,
                    stream.bufferedBytes.data(),
                    stream.bufferedByteCount,
                    0) < 0)
            {
                copyFailureMessage(failureMessage, "BCrypt could not hash coefficient bytes.");
                return false;
            }
            stream.bufferedByteCount = 0;
            return true;
        }

        bool appendHashBytes(
            Sha256Stream& stream,
            const unsigned char* bytes,
            std::size_t byteCount,
            std::array<char, JMSG_LENGTH_MAX>& failureMessage) noexcept
        {
            while (byteCount > 0)
            {
                const auto availableByteCount =
                    stream.bufferedBytes.size() - stream.bufferedByteCount;
                const auto appendedByteCount = byteCount < availableByteCount ?
                    byteCount : availableByteCount;
                std::memcpy(
                    stream.bufferedBytes.data() + stream.bufferedByteCount,
                    bytes,
                    appendedByteCount);
                stream.bufferedByteCount += static_cast<ULONG>(appendedByteCount);
                bytes += appendedByteCount;
                byteCount -= appendedByteCount;

                if (
                    stream.bufferedByteCount == stream.bufferedBytes.size() &&
                    !flushSha256Stream(stream, failureMessage))
                {
                    return false;
                }
            }
            return true;
        }

        bool appendUnsigned32(
            Sha256Stream& stream,
            std::uint32_t value,
            std::array<char, JMSG_LENGTH_MAX>& failureMessage) noexcept
        {
            const std::array<unsigned char, 4> bytes{
                static_cast<unsigned char>((value >> 24) & 0xff),
                static_cast<unsigned char>((value >> 16) & 0xff),
                static_cast<unsigned char>((value >> 8) & 0xff),
                static_cast<unsigned char>(value & 0xff),
            };
            return appendHashBytes(stream, bytes.data(), bytes.size(), failureMessage);
        }

        bool appendSigned16(
            Sha256Stream& stream,
            std::int16_t value,
            std::array<char, JMSG_LENGTH_MAX>& failureMessage) noexcept
        {
            const auto representation = static_cast<std::uint16_t>(value);
            const std::array<unsigned char, 2> bytes{
                static_cast<unsigned char>((representation >> 8) & 0xff),
                static_cast<unsigned char>(representation & 0xff),
            };
            return appendHashBytes(stream, bytes.data(), bytes.size(), failureMessage);
        }

        bool computeCoefficientDigest(
            const unsigned char* encodedJpeg,
            unsigned long encodedByteCount,
            std::array<unsigned char, sha256DigestByteCount>& digest,
            std::array<char, JMSG_LENGTH_MAX>& failureMessage)
        {
            Sha256Stream hashStream{};
            if (!initializeSha256Stream(hashStream, failureMessage))
            {
                return false;
            }

            jpeg_decompress_struct decoder{};
            JpegErrorManager errorManager{};
            bool decoderCreated = false;
            decoder.err = jpeg_std_error(&errorManager.base);
            errorManager.base.error_exit = handleJpegError;

            // No C++ object with a non-trivial destructor is created between
            // this setjmp and the final libjpeg call. That makes libjpeg's
            // documented longjmp error path safe and lets this branch release
            // every native handle explicitly.
            // MSVC warns about every setjmp in C++ even when, as here, only
            // trivially destructible state exists across the jump boundary.
#pragma warning(suppress : 4611)
            if (setjmp(errorManager.jumpBuffer) != 0)
            {
                if (decoderCreated)
                {
                    jpeg_destroy_decompress(&decoder);
                }
                destroySha256Stream(hashStream);
                failureMessage = errorManager.message;
                return false;
            }

            jpeg_create_decompress(&decoder);
            decoderCreated = true;
            jpeg_mem_src(&decoder, encodedJpeg, encodedByteCount);
            if (jpeg_read_header(&decoder, TRUE) != JPEG_HEADER_OK)
            {
                copyFailureMessage(failureMessage, "libjpeg did not recognize the JPEG header.");
                jpeg_destroy_decompress(&decoder);
                destroySha256Stream(hashStream);
                return false;
            }

            jvirt_barray_ptr* const coefficientArrays =
                jpeg_read_coefficients(&decoder);
            // sizeof includes the literal's terminating zero, which is part of
            // the versioned prefix documented by the public contract.
            constexpr unsigned char serializationPrefix[] =
                "jpg-spinner-coefficients-v1";
            if (!appendHashBytes(
                    hashStream,
                    serializationPrefix,
                    sizeof(serializationPrefix),
                    failureMessage) ||
                !appendUnsigned32(
                    hashStream,
                    static_cast<std::uint32_t>(decoder.num_components),
                    failureMessage))
            {
                jpeg_destroy_decompress(&decoder);
                destroySha256Stream(hashStream);
                return false;
            }

            for (int componentIndex = 0;
                 componentIndex < decoder.num_components;
                 ++componentIndex)
            {
                const auto& component = decoder.comp_info[componentIndex];
                if (
                    !appendUnsigned32(
                        hashStream,
                        static_cast<std::uint32_t>(componentIndex),
                        failureMessage) ||
                    !appendUnsigned32(
                        hashStream,
                        static_cast<std::uint32_t>(component.width_in_blocks),
                        failureMessage) ||
                    !appendUnsigned32(
                        hashStream,
                        static_cast<std::uint32_t>(component.height_in_blocks),
                        failureMessage))
                {
                    jpeg_destroy_decompress(&decoder);
                    destroySha256Stream(hashStream);
                    return false;
                }

                for (JDIMENSION blockRowIndex = 0;
                     blockRowIndex < component.height_in_blocks;
                     ++blockRowIndex)
                {
                    const JBLOCKARRAY blockRow =
                        (*decoder.mem->access_virt_barray)(
                            reinterpret_cast<j_common_ptr>(&decoder),
                            coefficientArrays[componentIndex],
                            blockRowIndex,
                            1,
                            FALSE);
                    for (JDIMENSION blockColumnIndex = 0;
                         blockColumnIndex < component.width_in_blocks;
                         ++blockColumnIndex)
                    {
                        for (int coefficientIndex = 0;
                             coefficientIndex < DCTSIZE2;
                             ++coefficientIndex)
                        {
                            const int coefficient =
                                blockRow[0][blockColumnIndex][coefficientIndex];
                            if (
                                coefficient < std::numeric_limits<std::int16_t>::min() ||
                                coefficient > std::numeric_limits<std::int16_t>::max())
                            {
                                copyFailureMessage(
                                    failureMessage,
                                    "A JPEG coefficient exceeds the canonical signed 16-bit domain.");
                                jpeg_destroy_decompress(&decoder);
                                destroySha256Stream(hashStream);
                                return false;
                            }
                            if (!appendSigned16(
                                    hashStream,
                                    static_cast<std::int16_t>(coefficient),
                                    failureMessage))
                            {
                                jpeg_destroy_decompress(&decoder);
                                destroySha256Stream(hashStream);
                                return false;
                            }
                        }
                    }
                }
            }

            jpeg_finish_decompress(&decoder);
            jpeg_destroy_decompress(&decoder);
            decoderCreated = false;

            if (!flushSha256Stream(hashStream, failureMessage) ||
                BCryptFinishHash(
                    hashStream.hash,
                    digest.data(),
                    static_cast<ULONG>(digest.size()),
                    0) < 0)
            {
                if (failureMessage[0] == '\0')
                {
                    copyFailureMessage(
                        failureMessage,
                        "BCrypt could not finalize the coefficient SHA-256 digest.");
                }
                destroySha256Stream(hashStream);
                return false;
            }

            destroySha256Stream(hashStream);
            return true;
        }
    }

    std::string CoefficientDigest::computeSha256Hex(
        std::span<const std::byte> encodedJpeg)
    {
        if (encodedJpeg.empty())
        {
            throw std::invalid_argument("The encoded JPEG must not be empty.");
        }
        if (encodedJpeg.size() > std::numeric_limits<unsigned long>::max())
        {
            throw std::invalid_argument("The JPEG exceeds libjpeg's in-memory source limit.");
        }

        std::array<unsigned char, sha256DigestByteCount> digest{};
        std::array<char, JMSG_LENGTH_MAX> failureMessage{};
        if (!computeCoefficientDigest(
                reinterpret_cast<const unsigned char*>(encodedJpeg.data()),
                static_cast<unsigned long>(encodedJpeg.size()),
                digest,
                failureMessage))
        {
            throw std::runtime_error(
                failureMessage[0] == '\0' ?
                    "Could not compute the JPEG coefficient digest." :
                    failureMessage.data());
        }

        constexpr std::string_view hexadecimalDigits = "0123456789abcdef";
        std::string hexadecimalDigest(digest.size() * 2, '0');
        for (std::size_t index = 0; index < digest.size(); ++index)
        {
            hexadecimalDigest[index * 2] = hexadecimalDigits[digest[index] >> 4];
            hexadecimalDigest[(index * 2) + 1] =
                hexadecimalDigits[digest[index] & 0x0f];
        }
        return hexadecimalDigest;
    }
}
