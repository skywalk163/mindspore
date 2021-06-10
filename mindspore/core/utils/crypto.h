/**
 * Copyright 2021 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MINDSPORE_CORE_UTILS_CRYPTO_H
#define MINDSPORE_CORE_UTILS_CRYPTO_H

#include <string>
#include <memory>

typedef unsigned char Byte;
namespace mindspore {
const size_t MAX_BLOCK_SIZE = 512 * 1024 * 1024;  // Maximum ciphertext segment, units is Byte
const unsigned int MAGIC_NUM = 0x7F3A5ED8;        // Magic number

std::unique_ptr<Byte[]> Encrypt(size_t *encrypt_len, Byte *plain_data, const size_t plain_len, const Byte *key,
                                const size_t key_len, const std::string &enc_mode);
std::unique_ptr<Byte[]> Decrypt(size_t *decrypt_len, const std::string &encrypt_data_path, const Byte *key,
                                const size_t key_len, const std::string &dec_mode);
std::unique_ptr<Byte[]> Decrypt(size_t *decrypt_len, const Byte *model_data, const size_t data_size, const Byte *key,
                                const size_t key_len, const std::string &dec_mode);
bool IsCipherFile(const std::string &file_path);
bool IsCipherFile(const Byte *model_data);
}  // namespace mindspore
#endif
