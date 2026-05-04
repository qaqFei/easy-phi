struct RegAPI {
    std::wstring appKey;

    RegAPI(std::wstring appKey) : appKey(std::move(appKey)) {}

private:
    std::wstring buildPath() const { return L"Software\\easy-phi\\" + appKey; }

    class KeyHandle {
        HKEY hKey_ = nullptr;
    public:
        KeyHandle(HKEY root, const std::wstring& path, REGSAM access) {
            LSTATUS result = RegOpenKeyExW(root, path.c_str(), 0, access, &hKey_);
            if (result == ERROR_FILE_NOT_FOUND) {
                DWORD disposition;
                result = RegCreateKeyExW(
                    root, path.c_str(), 0, nullptr,
                    REG_OPTION_NON_VOLATILE, access, nullptr,
                    &hKey_, &disposition
                );
            }
            if (result != ERROR_SUCCESS) {
                hKey_ = nullptr;
            }
        }
        ~KeyHandle() { if (hKey_) RegCloseKey(hKey_); }
        HKEY get() const { return hKey_; }
        explicit operator bool() const { return hKey_ != nullptr; }
    };

public:
    bool readDword(std::wstring key, DWORD& value) {
        KeyHandle hKey(HKEY_CURRENT_USER, buildPath(), KEY_READ);
        if (!hKey) return false;

        DWORD size = sizeof(value);
        DWORD type = 0;
        LSTATUS result = RegQueryValueExW(
            hKey.get(), key.c_str(), nullptr, &type,
            reinterpret_cast<BYTE*>(&value), &size
        );
        return (result == ERROR_SUCCESS && type == REG_DWORD);
    }

    void writeDword(std::wstring key, DWORD value) {
        KeyHandle hKey(HKEY_CURRENT_USER, buildPath(), KEY_WRITE);
        if (!hKey) return;

        RegSetValueExW(
            hKey.get(), key.c_str(), 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&value), sizeof(value)
        );
    }

    bool readDouble(std::wstring key, double& value) {
        KeyHandle hKey(HKEY_CURRENT_USER, buildPath(), KEY_READ);
        if (!hKey) return false;

        DWORD size = sizeof(value);
        DWORD type = 0;
        LSTATUS result = RegQueryValueExW(
            hKey.get(), key.c_str(), nullptr, &type,
            reinterpret_cast<BYTE*>(&value), &size
        );
        return (result == ERROR_SUCCESS && type == REG_BINARY);
    }

    void writeDouble(std::wstring key, double value) {
        KeyHandle hKey(HKEY_CURRENT_USER, buildPath(), KEY_WRITE);
        if (!hKey) return;

        RegSetValueExW(
            hKey.get(), key.c_str(), 0, REG_BINARY,
            reinterpret_cast<const BYTE*>(&value), sizeof(value)
        );
    }

    bool readString(std::wstring key, std::wstring& value) {
        KeyHandle hKey(HKEY_CURRENT_USER, buildPath(), KEY_READ);
        if (!hKey) return false;

        DWORD size = 0, type = 0;
        LSTATUS result = RegQueryValueExW(hKey.get(), key.c_str(), nullptr, &type, nullptr, &size);
        if (result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return false;

        value.resize(size / sizeof(wchar_t));
        result = RegQueryValueExW(
            hKey.get(), key.c_str(), nullptr, &type,
            reinterpret_cast<BYTE*>(value.data()), &size
        );
        if (result != ERROR_SUCCESS) return false;

        if (!value.empty() && value.back() == L'\0') value.pop_back();
        return true;
    }

    void writeString(std::wstring key, std::wstring value) {
        KeyHandle hKey(HKEY_CURRENT_USER, buildPath(), KEY_WRITE);
        if (!hKey) return;

        RegSetValueExW(
            hKey.get(), key.c_str(), 0, REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()),
            static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))
        );
    }

    bool readBool(std::wstring key, bool& value) {
        DWORD dw = 0;
        if (!readDword(key, dw)) return false;
        value = (dw != 0);
        return true;
    }

    void writeBool(std::wstring key, bool value) {
        writeDword(key, value ? 1 : 0);
    }

    template <typename T>
    bool readDword(std::wstring key, T& value) {
        DWORD dw = 0;
        if (!readDword(key, dw)) return false;
        value = (T)dw;
        return true;
    }

    template <typename T>
    void writeDword(std::wstring key, T value) {
        writeDword(key, (DWORD)value);
    }

    template <typename T>
    bool readDouble(std::wstring key, T& value) {
        double d = 0;
        if (!readDouble(key, d)) return false;
        value = (T)d;
        return true;
    }

    template <typename T>
    void writeDouble(std::wstring key, T value) {
        writeDouble(key, (double)value);
    }
};
