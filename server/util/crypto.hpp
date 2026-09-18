#pragma once
#include <string>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <chrono>
#include <jwt-cpp/jwt.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include "Exception.hpp"

namespace crypto{
    std::string getSalt()
    {
        const int salt_byte_len = 32;
        unsigned char raw_salt[salt_byte_len] = {0};

        int ret = RAND_bytes(raw_salt,salt_byte_len);
        if(ret != 1)
        {
            THROW_EXC(CryptoException,crypto_err::RAND_BYTES,"RAND_bytes failed, generate random salt error");
            return "";
        }

        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for(int i = 0;i < salt_byte_len;i++)
        {
            oss << std::setw(2) << int(raw_salt[i]);
        }

        return oss.str();
    }

    static std::vector<unsigned char> hex_to_bin(const std::string& hex)
    {
        std::vector<unsigned char> bin;
        for (size_t i = 0; i + 1 < hex.size(); i += 2)
        {
            std::string sub = hex.substr(i, 2);
            uint8_t val = static_cast<uint8_t>(strtol(sub.c_str(), nullptr, 16));
            bin.push_back(val);
        }
        return bin;
    }

    std::string pbkdf2_hash(const std::string& raw_pwd, const std::string& hex_salt, int iter = 100000)
    {
        std::vector<unsigned char> salt_bin = hex_to_bin(hex_salt);
        const int hash_byte_len = 32;
        unsigned char hash_out[hash_byte_len] = {0};

        int rc = PKCS5_PBKDF2_HMAC(
            raw_pwd.data(),
            static_cast<int>(raw_pwd.size()),
            salt_bin.data(),
            static_cast<int>(salt_bin.size()),
            iter,
            EVP_sha256(),
            hash_byte_len,
            hash_out
        );

        if (rc != 1)
        {
            THROW_EXC(CryptoException,crypto_err::PKCS5_PBKDF2_HMAC,"PKCS5_PBKDF2_HMAC compute failed");
            return "";
        }

        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (int i = 0; i < hash_byte_len; ++i)
        {
            oss << std::setw(2) << static_cast<int>(hash_out[i]);
        }
        return oss.str();
    }

    std::string jwt_issue(const std::string& sub, const std::string& issuer, const std::string& secret, long expire_sec)
    {
        auto now = std::chrono::system_clock::now();

        return jwt::create()
            .set_issuer(issuer)
            .set_subject(sub)
            .set_issued_at(now)
            .set_expires_at(now + std::chrono::seconds(expire_sec))
            .sign(jwt::algorithm::hs256{secret});
    }

    bool jwt_verify(const std::string& token, const std::string& issuer, const std::string& secret)
    {
        try
        {
            auto decoded = jwt::decode(token);

            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{secret})
                .with_issuer(issuer);

            verifier.verify(decoded);
            return true;
        }
        catch(const std::exception& e)
        {
            THROW_EXC(CryptoException,crypto_err::JWT_VERIfY,std::string("jwt_verify error : ") + e.what());
        }
        catch(...)
        {
            THROW_EXC(CryptoException,crypto_err::UNKNOWN,"jwt_verify Unknown Error!")
        }
        return false;
    }

    std::string jwt_get_sub(const std::string& token)
    {
        auto decoded = jwt::decode(token);
        return decoded.get_subject();
    }
}