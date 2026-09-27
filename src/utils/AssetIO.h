// Чтение/запись ассетов с прозрачным шифрованием.
// Формат: "AENC" + 8 байт соли + XOR-поток (splitmix64). В dev-проекте файлы сырые,
// в билде движок шифрует используемые ассеты — игрок не видит текстуры/сцены/звук как есть.
// (Это обфускация от «посмотреть глазами», не криптозащита от взлома.)
#pragma once
#include <string>
#include <vector>

namespace AssetIO {
    // Весь файл в память; если заголовок AENC — расшифровывает на лету
    std::string ReadAll(const std::string& path);
    bool ReadBytes(const std::string& path, std::vector<unsigned char>& out);
    // То же, но списком (пусто — файла нет или он пуст)
    std::vector<unsigned char> ReadBytes(const std::string& path);
    // Запись с шифрованием (Build Game)
    bool WriteEncrypted(const std::string& path, const std::string& data);
    std::string Encrypt(const std::string& plain);
    bool IsEncrypted(const std::vector<unsigned char>& bytes);
}
