#include "beatflow/quest/AndroidKeystore.hpp"

#include "UnityEngine/AndroidJNI.hpp"
#include "UnityEngine/jvalue.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"

#include <algorithm>
#include <initializer_list>
#include <string_view>
#include <utility>

namespace beatflow::quest {
namespace {

using Jni = UnityEngine::AndroidJNI;
using Pointer = System::IntPtr;
using Value = UnityEngine::jvalue;

bool isNull(Pointer pointer) {
    return pointer.m_value == nullptr;
}

Value objectValue(Pointer value) {
    Value argument{};
    argument.l = value;
    return argument;
}

Value integerValue(std::int32_t value) {
    Value argument{};
    argument.i = value;
    return argument;
}

ArrayW<Value> arguments(std::initializer_list<Value> values) {
    ArrayW<Value> result(il2cpp_array_size_t(values.size()));
    std::size_t index = 0;
    for (const auto& value : values) {
        result[index++] = value;
    }
    return result;
}

class LocalReferences {
  public:
    ~LocalReferences() {
        for (auto iterator = values_.rbegin(); iterator != values_.rend(); ++iterator) {
            Jni::DeleteLocalRef(*iterator);
        }
    }

    Pointer keep(Pointer value) {
        if (!isNull(value)) {
            values_.push_back(value);
        }
        return value;
    }

  private:
    std::vector<Pointer> values_;
};

Pointer javaString(LocalReferences& references, std::string_view value) {
    return references.keep(Jni::NewStringUTF(il2cpp_utils::newcsstr(value)));
}

Pointer javaByteArray(LocalReferences& references, std::span<const std::uint8_t> bytes) {
    auto array = references.keep(Jni::NewByteArray(static_cast<std::int32_t>(bytes.size())));
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        Jni::SetByteArrayElement(array, static_cast<std::int32_t>(index),
                                 static_cast<std::int8_t>(bytes[index]));
    }
    return array;
}

std::vector<std::uint8_t> nativeBytes(Pointer array) {
    const auto size = Jni::GetArrayLength(array);
    std::vector<std::uint8_t> result(static_cast<std::size_t>(std::max(size, 0)));
    for (std::int32_t index = 0; index < size; ++index) {
        result[static_cast<std::size_t>(index)] = Jni::GetByteArrayElement(array, index);
    }
    return result;
}

bool clearException() {
    const auto exception = Jni::ExceptionOccurred();
    if (isNull(exception)) {
        return false;
    }
    Jni::ExceptionClear();
    Jni::DeleteLocalRef(exception);
    return true;
}

ServiceError keystoreError(std::string action) {
    return {ErrorCode::Storage, "Android Keystore could not " + std::move(action) + ".", false, std::nullopt};
}

struct KeyObjects {
    Pointer keyStore{};
    Pointer key{};
};

Outcome<KeyObjects> loadKey(const std::string& alias, LocalReferences& refs) {
    Jni::AttachCurrentThread();
    const auto keyStoreClass = refs.keep(Jni::FindClass("java/security/KeyStore"));
    const auto keyStoreType = javaString(refs, "AndroidKeyStore");
    const auto getInstance =
        Jni::GetStaticMethodID(keyStoreClass, "getInstance", "(Ljava/lang/String;)Ljava/security/KeyStore;");
    const auto keyStore = refs.keep(
        Jni::CallStaticObjectMethod(keyStoreClass, getInstance, arguments({objectValue(keyStoreType)})));
    if (isNull(keyStore) || clearException()) {
        return Outcome<KeyObjects>::failure(keystoreError("open its secure key store"));
    }
    const auto load =
        Jni::GetMethodID(keyStoreClass, "load", "(Ljava/security/KeyStore$LoadStoreParameter;)V");
    Jni::CallVoidMethod(keyStore, load, arguments({objectValue({})}));
    if (clearException()) {
        return Outcome<KeyObjects>::failure(keystoreError("load its secure key store"));
    }

    const auto aliasString = javaString(refs, alias);
    const auto containsAlias = Jni::GetMethodID(keyStoreClass, "containsAlias", "(Ljava/lang/String;)Z");
    const bool exists =
        Jni::CallBooleanMethod(keyStore, containsAlias, arguments({objectValue(aliasString)}));
    if (clearException()) {
        return Outcome<KeyObjects>::failure(keystoreError("inspect its encryption key"));
    }

    if (!exists) {
        const auto builderClass =
            refs.keep(Jni::FindClass("android/security/keystore/KeyGenParameterSpec$Builder"));
        const auto builderConstructor = Jni::GetMethodID(builderClass, "<init>", "(Ljava/lang/String;I)V");
        const auto builder = refs.keep(
            Jni::NewObject(builderClass, builderConstructor,
                           arguments({objectValue(aliasString), integerValue(3)}))); // encrypt | decrypt

        const auto stringClass = refs.keep(Jni::FindClass("java/lang/String"));
        const auto gcmString = javaString(refs, "GCM");
        const auto gcmArray = refs.keep(Jni::NewObjectArray(1, stringClass, gcmString));
        const auto setBlockModes =
            Jni::GetMethodID(builderClass, "setBlockModes",
                             "([Ljava/lang/String;)Landroid/security/keystore/KeyGenParameterSpec$Builder;");
        refs.keep(Jni::CallObjectMethod(builder, setBlockModes, arguments({objectValue(gcmArray)})));

        const auto paddingString = javaString(refs, "NoPadding");
        const auto paddingArray = refs.keep(Jni::NewObjectArray(1, stringClass, paddingString));
        const auto setPaddings =
            Jni::GetMethodID(builderClass, "setEncryptionPaddings",
                             "([Ljava/lang/String;)Landroid/security/keystore/KeyGenParameterSpec$Builder;");
        refs.keep(Jni::CallObjectMethod(builder, setPaddings, arguments({objectValue(paddingArray)})));

        const auto build =
            Jni::GetMethodID(builderClass, "build", "()Landroid/security/keystore/KeyGenParameterSpec;");
        const auto specification = refs.keep(Jni::CallObjectMethod(builder, build, arguments({})));

        const auto generatorClass = refs.keep(Jni::FindClass("javax/crypto/KeyGenerator"));
        const auto aes = javaString(refs, "AES");
        const auto provider = javaString(refs, "AndroidKeyStore");
        const auto getGenerator =
            Jni::GetStaticMethodID(generatorClass, "getInstance",
                                   "(Ljava/lang/String;Ljava/lang/String;)Ljavax/crypto/KeyGenerator;");
        const auto generator = refs.keep(Jni::CallStaticObjectMethod(
            generatorClass, getGenerator, arguments({objectValue(aes), objectValue(provider)})));
        const auto initialize =
            Jni::GetMethodID(generatorClass, "init", "(Ljava/security/spec/AlgorithmParameterSpec;)V");
        Jni::CallVoidMethod(generator, initialize, arguments({objectValue(specification)}));
        const auto generate = Jni::GetMethodID(generatorClass, "generateKey", "()Ljavax/crypto/SecretKey;");
        refs.keep(Jni::CallObjectMethod(generator, generate, arguments({})));
        if (clearException()) {
            return Outcome<KeyObjects>::failure(keystoreError("create its encryption key"));
        }
    }

    const auto getKey =
        Jni::GetMethodID(keyStoreClass, "getKey", "(Ljava/lang/String;[C)Ljava/security/Key;");
    const auto key = refs.keep(
        Jni::CallObjectMethod(keyStore, getKey, arguments({objectValue(aliasString), objectValue({})})));
    if (isNull(key) || clearException()) {
        return Outcome<KeyObjects>::failure(keystoreError("read its encryption key"));
    }
    return Outcome<KeyObjects>::success({keyStore, key});
}

} // namespace

AndroidKeystore::AndroidKeystore(std::string alias) : alias_(std::move(alias)) {}

Outcome<EncryptedSecret> AndroidKeystore::encrypt(std::span<const std::uint8_t> plaintext) const {
    LocalReferences refs;
    auto keys = loadKey(alias_, refs);
    if (!keys) {
        return Outcome<EncryptedSecret>::failure(keys.error());
    }

    const auto cipherClass = refs.keep(Jni::FindClass("javax/crypto/Cipher"));
    const auto transformation = javaString(refs, "AES/GCM/NoPadding");
    const auto getInstance =
        Jni::GetStaticMethodID(cipherClass, "getInstance", "(Ljava/lang/String;)Ljavax/crypto/Cipher;");
    const auto cipher = refs.keep(
        Jni::CallStaticObjectMethod(cipherClass, getInstance, arguments({objectValue(transformation)})));
    const auto initialize = Jni::GetMethodID(cipherClass, "init", "(ILjava/security/Key;)V");
    Jni::CallVoidMethod(cipher, initialize, arguments({integerValue(1), objectValue(keys.value().key)}));
    const auto getInitializationVector = Jni::GetMethodID(cipherClass, "getIV", "()[B");
    const auto initializationVector =
        refs.keep(Jni::CallObjectMethod(cipher, getInitializationVector, arguments({})));
    const auto input = javaByteArray(refs, plaintext);
    const auto finish = Jni::GetMethodID(cipherClass, "doFinal", "([B)[B");
    const auto ciphertext = refs.keep(Jni::CallObjectMethod(cipher, finish, arguments({objectValue(input)})));
    if (isNull(initializationVector) || isNull(ciphertext) || clearException()) {
        return Outcome<EncryptedSecret>::failure(keystoreError("encrypt account data"));
    }
    return Outcome<EncryptedSecret>::success({nativeBytes(initializationVector), nativeBytes(ciphertext)});
}

Outcome<std::vector<std::uint8_t>> AndroidKeystore::decrypt(const EncryptedSecret& secret) const {
    if (secret.initializationVector.empty() || secret.ciphertext.empty()) {
        return Outcome<std::vector<std::uint8_t>>::failure(
            {ErrorCode::Storage, "The encrypted account data is incomplete.", false, std::nullopt});
    }
    LocalReferences refs;
    auto keys = loadKey(alias_, refs);
    if (!keys) {
        return Outcome<std::vector<std::uint8_t>>::failure(keys.error());
    }

    const auto cipherClass = refs.keep(Jni::FindClass("javax/crypto/Cipher"));
    const auto transformation = javaString(refs, "AES/GCM/NoPadding");
    const auto getInstance =
        Jni::GetStaticMethodID(cipherClass, "getInstance", "(Ljava/lang/String;)Ljavax/crypto/Cipher;");
    const auto cipher = refs.keep(
        Jni::CallStaticObjectMethod(cipherClass, getInstance, arguments({objectValue(transformation)})));

    const auto specificationClass = refs.keep(Jni::FindClass("javax/crypto/spec/GCMParameterSpec"));
    const auto iv = javaByteArray(refs, secret.initializationVector);
    const auto specificationConstructor = Jni::GetMethodID(specificationClass, "<init>", "(I[B)V");
    const auto specification = refs.keep(Jni::NewObject(specificationClass, specificationConstructor,
                                                        arguments({integerValue(128), objectValue(iv)})));
    const auto initialize = Jni::GetMethodID(
        cipherClass, "init", "(ILjava/security/Key;Ljava/security/spec/AlgorithmParameterSpec;)V");
    Jni::CallVoidMethod(
        cipher, initialize,
        arguments({integerValue(2), objectValue(keys.value().key), objectValue(specification)}));
    const auto input = javaByteArray(refs, secret.ciphertext);
    const auto finish = Jni::GetMethodID(cipherClass, "doFinal", "([B)[B");
    const auto plaintext = refs.keep(Jni::CallObjectMethod(cipher, finish, arguments({objectValue(input)})));
    if (isNull(plaintext) || clearException()) {
        return Outcome<std::vector<std::uint8_t>>::failure(keystoreError("decrypt account data"));
    }
    return Outcome<std::vector<std::uint8_t>>::success(nativeBytes(plaintext));
}

} // namespace beatflow::quest
