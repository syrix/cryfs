#include <google/gtest/gtest.h>
#include <messmer/cpp-utils/data/DataFixture.h>
#include <messmer/cpp-utils/crypto/symmetric/ciphers.h>
#include <boost/optional/optional_io.hpp>
#include "../../../src/config/crypto/CryConfigEncryptor.h"

using std::ostream;
using cpputils::unique_ref;
using cpputils::make_unique_ref;
using cpputils::DataFixture;
using cpputils::Data;
using cpputils::DerivedKeyConfig;
using cpputils::DerivedKey;
using cpputils::AES128_CFB;
using cpputils::AES256_GCM;
using cpputils::Twofish256_GCM;
using cpputils::Twofish128_CFB;
using boost::none;
using namespace cryfs;

// This is needed for google test
namespace boost {
    inline ostream &operator<<(ostream &stream, const CryConfigEncryptor::Decrypted &) {
        return stream << "CryConfigEncryptor::Decrypted()";
    }
}

class CryConfigEncryptorTest: public ::testing::Test {
public:

    unique_ref<CryConfigEncryptor> makeEncryptor() {
        return make_unique_ref<CryConfigEncryptor>(_derivedKey());
    }

    Data changeInnerCipherFieldTo(Data data, const string &newCipherName) {
        InnerConfig innerConfig = _decryptInnerConfig(data);
        innerConfig.cipherName = newCipherName;
        return _encryptInnerConfig(innerConfig);
    }

private:
    DerivedKey<CryConfigEncryptor::MaxTotalKeySize> _derivedKey() {
        auto salt = DataFixture::generate(128, 2);
        auto keyConfig = DerivedKeyConfig(std::move(salt), 1024, 1, 2);
        auto key = DataFixture::generateFixedSize<CryConfigEncryptor::MaxTotalKeySize>(3);
        return DerivedKey<CryConfigEncryptor::MaxTotalKeySize>(std::move(keyConfig), std::move(key));
    }

    unique_ref<OuterEncryptor> _outerEncryptor() {
        auto outerKey = _derivedKey().key().take<CryConfigEncryptor::OuterKeySize>();
        return make_unique_ref<OuterEncryptor>(outerKey, _derivedKey().config());
    }

    InnerConfig _decryptInnerConfig(const Data &data) {
        OuterConfig outerConfig = OuterConfig::deserialize(data).value();
        Data serializedInnerConfig = _outerEncryptor()->decrypt(outerConfig).value();
        return InnerConfig::deserialize(serializedInnerConfig).value();
    }

    Data _encryptInnerConfig(const InnerConfig &innerConfig) {
        Data serializedInnerConfig = innerConfig.serialize();
        OuterConfig outerConfig = _outerEncryptor()->encrypt(serializedInnerConfig);
        return outerConfig.serialize();
    }
};

TEST_F(CryConfigEncryptorTest, EncryptAndDecrypt_Data_AES) {
    auto encryptor = makeEncryptor();
    Data encrypted = encryptor->encrypt(DataFixture::generate(400), AES256_GCM::NAME);
    auto decrypted = encryptor->decrypt(encrypted).value();
    EXPECT_EQ(DataFixture::generate(400), decrypted.data);
}

TEST_F(CryConfigEncryptorTest, EncryptAndDecrypt_Data_Twofish) {
    auto encryptor = makeEncryptor();
    Data encrypted = encryptor->encrypt(DataFixture::generate(400), Twofish128_CFB::NAME);
    auto decrypted = encryptor->decrypt(encrypted).value();
    EXPECT_EQ(DataFixture::generate(400), decrypted.data);
}

TEST_F(CryConfigEncryptorTest, EncryptAndDecrypt_Cipher_AES) {
    auto encryptor = makeEncryptor();
    Data encrypted = encryptor->encrypt(DataFixture::generate(400), AES256_GCM::NAME);
    auto decrypted = encryptor->decrypt(encrypted).value();
    EXPECT_EQ(AES256_GCM::NAME, decrypted.cipherName);
}

TEST_F(CryConfigEncryptorTest, EncryptAndDecrypt_Cipher_Twofish) {
    auto encryptor = makeEncryptor();
    Data encrypted = encryptor->encrypt(DataFixture::generate(400), Twofish128_CFB::NAME);
    auto decrypted = encryptor->decrypt(encrypted).value();
    EXPECT_EQ(Twofish128_CFB::NAME, decrypted.cipherName);
}

TEST_F(CryConfigEncryptorTest, EncryptAndDecrypt_EmptyData) {
    auto encryptor = makeEncryptor();
    Data encrypted = encryptor->encrypt(Data(0), AES256_GCM::NAME);
    auto decrypted = encryptor->decrypt(encrypted).value();
    EXPECT_EQ(Data(0), decrypted.data);
}

TEST_F(CryConfigEncryptorTest, InvalidCiphertext) {
    auto encryptor = makeEncryptor();
    Data encrypted = encryptor->encrypt(DataFixture::generate(400), AES256_GCM::NAME);
    *(char*)encrypted.data() = *(char*)encrypted.data()+1; //Modify ciphertext
    auto decrypted = encryptor->decrypt(encrypted);
    EXPECT_EQ(none, decrypted);
}

TEST_F(CryConfigEncryptorTest, DoesntEncryptWhenTooLarge) {
    auto encryptor = makeEncryptor();
    EXPECT_THROW(
            encryptor->encrypt(DataFixture::generate(2000), AES256_GCM::NAME),
            std::runtime_error
    );
}

TEST_F(CryConfigEncryptorTest, EncryptionIsFixedSize) {
    auto encryptor = makeEncryptor();
    Data encrypted1 = encryptor->encrypt(DataFixture::generate(100), AES128_CFB::NAME);
    Data encrypted2 = encryptor->encrypt(DataFixture::generate(200), Twofish256_GCM::NAME);
    Data encrypted3 = encryptor->encrypt(Data(0), AES256_GCM::NAME);

    EXPECT_EQ(encrypted1.size(), encrypted2.size());
    EXPECT_EQ(encrypted1.size(), encrypted3.size());
}

TEST_F(CryConfigEncryptorTest, SpecifiedInnerCipherIsUsed) {
    //Tests that it can't be decrypted if the inner cipher field stores the wrong cipher
    auto encryptor = makeEncryptor();
    Data encrypted = encryptor->encrypt(DataFixture::generate(400), AES256_GCM::NAME);
    encrypted = changeInnerCipherFieldTo(std::move(encrypted), Twofish256_GCM::NAME);
    auto decrypted = encryptor->decrypt(encrypted);
    EXPECT_EQ(none, decrypted);
}
