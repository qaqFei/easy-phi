#ifndef EASY_PHI_HPP
#define EASY_PHI_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <unordered_map>
#include <cmath>
#include <stdarg.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <functional>
#include <numbers>
#include <random>
#include <filesystem>
#include <ranges>
#include <unordered_set>
#include <string_view>
#include <utility>
#include <atomic>
#include <thread>
#include <map>
#include <cstring>
#include <cpuid.h>
#include <cstdlib>
#include <list>
#include <cassert>

namespace easy_phi {

using ep_u8 = uint8_t;
using ep_u16 = uint16_t;
using ep_u32 = uint32_t;
using ep_u64 = uint64_t;

using ep_i8 = int8_t;
using ep_i16 = int16_t;
using ep_i32 = int32_t;
using ep_i64 = int64_t;

using ep_f32 = float;
using ep_f64 = double;

static ep_u64 globalCounter = 1;

ep_u64 reqGlobalCounter() {
    return globalCounter++;
}

bool cpuHasSSE2() {
    unsigned int eax, ebx, ecx, edx;
    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        return false;
    }
    return (edx & (1u << 26)) != 0;
}

template <typename T1, typename T2>
T1 typed_clamp(T2 v) noexcept {
    return (T1)std::clamp(v, (T2)std::numeric_limits<T1>::min(), (T2)std::numeric_limits<T1>::max());
}

#ifdef _WIN32
    inline void* ep_aligned_alloc(size_t alignment, size_t size) { return _aligned_malloc(size, alignment); }
    inline void ep_aligned_free(void* ptr) { _aligned_free(ptr); }
#else
    inline void* ep_aligned_alloc(size_t alignment, size_t size) { return std::aligned_alloc(alignment, size); }
    inline void ep_aligned_free(void* ptr) { free(ptr); }
#endif

template <typename T, size_t Alignment>
struct AlignedAllocator {
    /* !docs
    The aligned allocator.
    */

    using value_type = T;

    AlignedAllocator() = default;
    template <typename U>
    AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

    template <typename U>
    struct rebind {
        using other = AlignedAllocator<U, Alignment>;
    };

    T* allocate(std::size_t n) {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::bad_array_new_length();

        std::size_t bytes = ((n * sizeof(T) + Alignment - 1) / Alignment) * Alignment;
        void* ptr = ep_aligned_alloc(Alignment, bytes);
        if (!ptr)
            throw std::bad_alloc();
        return static_cast<T*>(ptr);
    }

    void deallocate(T* ptr, std::size_t) noexcept { ep_aligned_free(ptr); }
};

template <typename T, size_t A1, typename U, size_t A2>
bool operator==(const AlignedAllocator<T, A1>&, const AlignedAllocator<U, A2>&) noexcept { return A1 == A2; }
template <typename T, size_t A1, typename U, size_t A2>
bool operator!=(const AlignedAllocator<T, A1>&, const AlignedAllocator<U, A2>&) noexcept { return A1 != A2; }

template <typename T, size_t Alignment>
using aligned_vector = std::vector<T, AlignedAllocator<T, Alignment>>;

ep_f64 globalTimer() {
    /* !docs
    Get the current time in seconds since the program started.
    */

    return std::chrono::duration<ep_f64>(
        std::chrono::steady_clock::now()
        .time_since_epoch()
    ).count();
}

std::string toUtfChar(ep_u16 n, ep_u16 n2 = 0) {
    /* !docs
    Convert a codepoint to a UTF-8 string.
    */

    ep_u32 codepoint;
    
    if (n >= 0xD800 && n <= 0xDBFF) {
        if (n2 >= 0xDC00 && n2 <= 0xDFFF) {
            codepoint = 0x10000 + ((n - 0xD800) << 10) | (n2 - 0xDC00);
        } else {
            return "\xEF\xBF\xBD";
        }
    } else if (n >= 0xDC00 && n <= 0xDFFF) {
        return "\xEF\xBF\xBD";
    } else {
        codepoint = n;
    }
    
    std::string result;
    
    if (codepoint <= 0x7F) {
        result.push_back((char)(codepoint));
    } 
    else if (codepoint <= 0x7FF) {
        result.push_back((char)(0xC0 | (codepoint >> 6)));
        result.push_back((char)(0x80 | (codepoint & 0x3F)));
    } 
    else if (codepoint <= 0xFFFF) {
        result.push_back((char)(0xE0 | (codepoint >> 12)));
        result.push_back((char)(0x80 | ((codepoint >> 6) & 0x3F)));
        result.push_back((char)(0x80 | (codepoint & 0x3F)));
    } 
    else {
        result.push_back((char)(0xF0 | (codepoint >> 18)));
        result.push_back((char)(0x80 | ((codepoint >> 12) & 0x3F)));
        result.push_back((char)(0x80 | ((codepoint >> 6) & 0x3F)));
        result.push_back((char)(0x80 | (codepoint & 0x3F)));
    }
    
    return result;
}

std::string formatToStdString(const char* fmt, ...) {
    /* !docs
    Format a string with the same syntax as `printf`.
    */

    va_list args;

    va_start(args, fmt);
    int len = vsnprintf(nullptr, 0, fmt, args);
    va_end(args);

    if (len < 0) return "";

    std::vector<char> buf(len + 1);
    va_start(args, fmt);
    vsnprintf(buf.data(), buf.size(), fmt, args);
    va_end(args);

    return std::string(buf.data(), len);
}

void checkBoolAndThrow(bool condition, const std::string& msg, const std::string& prefix = "") {
    if (!condition) {
        if (prefix.empty()) {
            throw std::runtime_error(msg);
        } else {
            throw std::runtime_error(prefix + ": " + msg);
        }
    }
}

#ifdef EASY_PHI_IS_RELEASE
    #define ep_assert(condition, msg) ((void)0)
#else
    #define ep_assert(condition, msg) \
        do { \
            if (!(condition)) { \
                std::cerr << "Easy phi assertion failed: " << (msg) << std::endl; \
                std::abort(); \
            } \
        } while (0)
#endif

template <typename T>
struct ep_sp {
    /* !docs
    The small pointer class.
    */

    struct RefCnt {
        T* ptr;
        std::atomic<int> count{1};
        
        explicit RefCnt(T* p) : ptr(p) {}
        void ref() noexcept { ++count; }
        void unref() noexcept {
            if (--count == 0) {
                delete ptr;
                delete this;
            }
        }
    };
    
    RefCnt* fCtrl;

    explicit ep_sp(RefCnt* ctrl) : fCtrl(ctrl) {}
    using element_type = T;
    constexpr ep_sp() noexcept : fCtrl(nullptr) {}
    constexpr ep_sp(std::nullptr_t) noexcept : fCtrl(nullptr) {}
    explicit ep_sp(T* ptr) : fCtrl(ptr ? new RefCnt(ptr) : nullptr) {}
    ep_sp(const ep_sp& o) noexcept : fCtrl(o.fCtrl) { if (fCtrl) fCtrl->ref(); }
    ep_sp(ep_sp&& o) noexcept : fCtrl(o.fCtrl) { o.fCtrl = nullptr; }
    template <typename U> ep_sp(const ep_sp<U>& o) noexcept : fCtrl(o.fCtrl) { if (fCtrl) fCtrl->ref(); }
    template <typename U> ep_sp(ep_sp<U>&& o) noexcept : fCtrl(o.release_ctrl()) {}
    ~ep_sp() { if (fCtrl) fCtrl->unref(); }

    ep_sp& operator=(const ep_sp& o) noexcept {
        if (o.fCtrl != fCtrl) {
            if (o.fCtrl) o.fCtrl->ref();
            auto* old = fCtrl;
            fCtrl = o.fCtrl;
            if (old) old->unref();
        }
        return *this;
    }
    
    ep_sp& operator=(ep_sp&& o) noexcept {
        if (o.fCtrl != fCtrl) {
            auto* old = fCtrl;
            fCtrl = o.fCtrl;
            o.fCtrl = nullptr;
            if (old) old->unref();
        }
        return *this;
    }
    
    ep_sp& operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }

    T& operator*() const { return *fCtrl->ptr; }
    T* operator->() const { return fCtrl->ptr; }
    T* get() const noexcept { return fCtrl ? fCtrl->ptr : nullptr; }
    explicit operator bool() const noexcept { return fCtrl != nullptr; }

    T* release() noexcept {
        if (!fCtrl) return nullptr;
        auto* p = fCtrl->ptr;
        fCtrl->ptr = nullptr;
        fCtrl->unref();
        fCtrl = nullptr;
        return p;
    }
    
    void reset(T* ptr = nullptr) noexcept {
        if (fCtrl && fCtrl->ptr == ptr) return;
        auto* old = fCtrl;
        fCtrl = ptr ? new RefCnt(ptr) : nullptr;
        if (old) old->unref();
    }
    
    void swap(ep_sp& o) noexcept {
        std::swap(fCtrl, o.fCtrl);
    }

    auto* release_ctrl() noexcept {
        auto* c = fCtrl;
        fCtrl = nullptr;
        return c;
    }
};

template <typename T, typename U>
bool operator==(const ep_sp<T>& a, const ep_sp<U>& b) noexcept { return a.get() == b.get(); }
template <typename T>
bool operator==(const ep_sp<T>& a, std::nullptr_t) noexcept { return !a; }
template <typename T>
bool operator==(std::nullptr_t, const ep_sp<T>& a) noexcept { return !a; }

struct HashBucket {
    /* !docs
    A FNV-1a hash bucket, used to generate a hash from a sequence of numbers and booleans.
    */

    ep_u64 hash = 0xcbf29ce484222325ULL;
    
    static constexpr ep_u64 FNV_PRIME = 0x100000001b3ULL;
    
    void mix(ep_u64 value) noexcept {
        hash ^= value;
        hash *= FNV_PRIME;
    }
    
    template <typename T>
    void submitNumber(T v) noexcept {
        static_assert(std::is_arithmetic_v<T>, "T must be numeric");
        
        if constexpr (std::is_floating_point_v<T>) {
            if (v == 0) v = 0;
        }
        
        const ep_u8* bytes = reinterpret_cast<const ep_u8*>(&v);
        for (ep_u64 i = 0; i < sizeof(T); i++) {
            mix(bytes[i]);
        }
    }
    
    void submitBool(bool b) noexcept { mix(b ? 1 : 0); }

    template <typename T>
    void submitOptionalNumber(std::optional<T> v) noexcept {
        if (v.has_value()) {
            submitBool(true);
            submitNumber(v.value());
        } else submitBool(false);
    }
    
    ep_u64 getHash() const noexcept { return hash; }
};

template <typename T1, typename T2>
struct SKVCache {
    /* !docs
    A simple key-value cache.
    */

    T1 key;
    T2 value;

    template <typename F>
    [[gnu::always_inline, gnu::hot]]
    const T2& get(const T1& k, F&& reseter) noexcept {
        /* !docs
        Gets the value from the cache.
        If the key is different from the cached key, the value is reset by reseter function and the key is updated.
        */

        if (__builtin_expect(key != k, 0)) {
            key = k;
            value = reseter();
        }

        return value;
    }
};

struct Data {
    /* !docs
    A simple byte array, used to store data.
    */

    std::vector<ep_u8> data;

    static Data MakeFromFile(const std::string& fn) {
        std::ifstream file(std::filesystem::path((const char8_t*)fn.c_str()), std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("failed to read file");

        ep_u64 size = file.tellg();
        file.seekg(0);
        std::vector<ep_u8> buffer(size);
        if (!file.read((char*)buffer.data(), size)) throw std::runtime_error("failed to read file");
        file.close();

        return { .data = std::move(buffer) };
    }

    static bool MakeFromFile(Data& data, const std::string& fn) {
        try {
            data = MakeFromFile(fn);
            return true;
        } catch (...) {
            return false;
        }
    }

    std::string toString() const {
        return std::string((char*)data.data(), data.size());
    }

    ep_u64 getHash() const {
        /* !docs
        Returns a hash of the data, by submitting each byte to a `@HashBucket`.
        */

        HashBucket bucket;
        for (ep_u8 byte : data) bucket.submitNumber(byte);
        return bucket.getHash();
    }

    bool isStartsWith(const Data& other) const noexcept {
        if (data.size() < other.data.size()) return false;
        return std::memcmp(data.data(), other.data.data(), other.data.size()) == 0;
    }

    bool isEndsWith(const Data& other) const noexcept {
        if (data.size() < other.data.size()) return false;
        return std::memcmp(data.data() + data.size() - other.data.size(), other.data.data(), other.data.size()) == 0;
    }

    bool isStartsWith(const std::string& other) const noexcept {
        if (data.size() < other.size()) return false;
        return std::memcmp(data.data(), other.data(), other.size()) == 0;
    }

    bool isEndsWith(const std::string& other) const noexcept {
        if (data.size() < other.size()) return false;
        return std::memcmp(data.data() + data.size() - other.size(), other.data(), other.size()) == 0;
    }
};

enum class ByteEndian {
    Native,
    Little,
    Big
};

template<ByteEndian E>
struct ByteWriter {
    /* !docs
    A class for writing bytes to a vector.
    */

    std::vector<ep_u8> data;

    static ep_sp<ByteWriter<E>> Make() {
        auto* writer = new ByteWriter<E>();
        return ep_sp<ByteWriter<E>>(writer);
    }

    void writeBytes(const Data& data) noexcept {
        this->data.insert(this->data.end(), data.data.begin(), data.data.end());
    }

    void writeBytes(const ep_u8* data, ep_u64 size) noexcept {
        this->data.insert(this->data.end(), data, data + size);
    }

    void writeBytes(const std::string& data) noexcept {
        writeBytes((const ep_u8*)data.data(), data.size());
    }

    void writeBytes(const std::vector<ep_u8>& data) noexcept {
        writeBytes(data.data(), data.size());
    }

    template<typename T>
    static T byte_swap(T val) noexcept {
        if constexpr (sizeof(T) == 1) return val;
        else if constexpr (sizeof(T) == 2) {
            ep_u16 ret;
            memcpy(&ret, &val, sizeof(T));
            ret = __builtin_bswap16(ret);
            memcpy(&val, &ret, sizeof(T));
            return val;
        } else if constexpr (sizeof(T) == 4) {
            ep_u32 ret;
            memcpy(&ret, &val, sizeof(T));
            ret = __builtin_bswap32(ret);
            memcpy(&val, &ret, sizeof(T));
            return val;
        } else if constexpr (sizeof(T) == 8) {
            ep_u64 ret;
            memcpy(&ret, &val, sizeof(T));
            ret = __builtin_bswap64(ret);
            memcpy(&val, &ret, sizeof(T));
            return val;
        } else {
            static_assert(!sizeof(T), "Unsupported size");
            return val;
        }
    }

    template<typename T>
    static T from_native(T val) noexcept {
        if constexpr (E == ByteEndian::Native) return val;
        #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            else if constexpr (E == ByteEndian::Little) return val;
            else return byte_swap(val);
        #else
            else if constexpr (E == ByteEndian::Big) return val;
            else return byte_swap(val);
        #endif
    }

    template<typename T>
    void write(T value) noexcept {
        static_assert(std::is_trivially_copyable_v<T>);
        T write_val = from_native(value);
        writeBytes((ep_u8*)&write_val, sizeof(T));
    }

    Data toData() const {
        return { .data = data };
    }
};

struct JsonNode {
    /* !docs
    A JSON node.
    */

    enum class EnumType {
        String, Number, Bool, Array, Object, Null
    };

    EnumType type;
    std::variant<
        std::monostate,
        std::string,
        ep_f64,
        bool,
        std::vector<JsonNode>,
        std::unordered_map<std::string, JsonNode>
    > value;

    static JsonNode MakeString(const std::string& str) noexcept {
        return JsonNode {
            .type = EnumType::String,
            .value = str
        };
    }

    static JsonNode MakeStringMove(std::string&& str) noexcept {
        return JsonNode {
            .type = EnumType::String,
            .value = std::move(str)
        };
    }

    static JsonNode MakeNumber(ep_f64 num) noexcept {
        return JsonNode {
            .type = EnumType::Number,
            .value = num
        };
    }

    static JsonNode MakeBool(bool b) noexcept {
        return JsonNode {
            .type = EnumType::Bool,
            .value = b
        };
    }

    static JsonNode MakeArray() noexcept {
        return JsonNode {
            .type = EnumType::Array,
            .value = std::vector<JsonNode>()
        };
    }

    static JsonNode MakeArray(const std::vector<JsonNode>& arr) noexcept {
        return JsonNode {
            .type = EnumType::Array,
            .value = arr
        };
    }

    static JsonNode MakeArrayMove(std::vector<JsonNode>&& arr) noexcept {
        return JsonNode {
            .type = EnumType::Array,
            .value = std::move(arr)
        };
    }

    static JsonNode MakeObject() noexcept {
        return JsonNode {
            .type = EnumType::Object,
            .value = std::unordered_map<std::string, JsonNode>()
        };
    }

    static JsonNode MakeObject(const std::unordered_map<std::string, JsonNode>& obj) noexcept {
        return JsonNode {
            .type = EnumType::Object,
            .value = obj
        };
    }

    static JsonNode MakeObjectMove(std::unordered_map<std::string, JsonNode>&& obj) noexcept {
        return JsonNode {
            .type = EnumType::Object,
            .value = std::move(obj)
        };
    }

    static JsonNode MakeNull() noexcept {
        return JsonNode {
            .type = EnumType::Null,
            .value = std::monostate{}
        };
    }

    bool isString() const noexcept { return type == EnumType::String; }
    bool isNumber() const noexcept { return type == EnumType::Number; }
    bool isBool() const noexcept { return type == EnumType::Bool; }
    bool isArray() const noexcept { return type == EnumType::Array; }
    bool isObject() const noexcept { return type == EnumType::Object; }
    bool isNull() const noexcept { return type == EnumType::Null; }

    std::string& getString() noexcept { return std::get<std::string>(value); }
    const std::string& getString() const noexcept { return std::get<std::string>(value); }
    ep_f64 getNumber() const noexcept { return std::get<ep_f64>(value); }
    bool getBool() const noexcept { return std::get<bool>(value); }
    std::vector<JsonNode>& getArray() noexcept { return std::get<std::vector<JsonNode>>(value); }
    const std::vector<JsonNode>& getArray() const noexcept { return std::get<std::vector<JsonNode>>(value); }
    std::unordered_map<std::string, JsonNode>& getObject() noexcept { return std::get<std::unordered_map<std::string, JsonNode>>(value); }
    const std::unordered_map<std::string, JsonNode>& getObject() const noexcept { return std::get<std::unordered_map<std::string, JsonNode>>(value); }
    
    struct StringReader {
        std::string_view str;
        ep_u64 pos;

        StringReader(std::string_view str) : str(str), pos(0) {}

        void eatWhitespace() {
            while (pos < str.size() && (str[pos] == ' ' || str[pos] == '\n' || str[pos] == '\t' || str[pos] == '\r')) {
                pos++;
            }
        }

        bool nextIs(const char c) {
            return pos < str.size() && str[pos] == c;
        }

        bool nextIsAny(const std::string& s) {
            for (ep_u64 i = 0; i < s.size(); i++) {
                if (nextIs(s[i])) return true;
            }
            return false;
        }

        bool nextIsSub(const std::string& s) {
            return pos + s.size() <= str.size() && str.substr(pos, s.size()) == s;
        }

        bool nextIsSubAny(const std::vector<std::string>& ss) {
            for (const auto& s : ss) {
                if (nextIsSub(s)) return true;
            }
            return false;
        }

        std::string getNextCharToString() {
            return pos < str.size() ? (std::string() + str[pos++]) : "";
        }

        std::string generatePositionString() {
            return "at " + std::to_string(pos) + " of " + std::to_string(str.size());
        }

        bool eof() {
            return pos >= str.size();
        }

        bool readUnicodeEscape(ep_u16* dst) {
            if (pos + 4 > str.size()) return false;

            auto c1 = str[pos++];
            auto c2 = str[pos++];
            auto c3 = str[pos++];
            auto c4 = str[pos++];

            if ('0' <= c1 && c1 <= '9') {
                *dst = (ep_u16)(c1 - '0') << 12;
            } else if ('A' <= c1 && c1 <= 'F') {
                *dst = (ep_u16)(c1 - 'A' + 10) << 12;
            } else if ('a' <= c1 && c1 <= 'f') {
                *dst = (ep_u16)(c1 - 'a' + 10) << 12;
            } else return false;

            if ('0' <= c2 && c2 <= '9') {
                *dst |= (ep_u16)(c2 - '0') << 8;
            } else if ('A' <= c2 && c2 <= 'F') {
                *dst |= (ep_u16)(c2 - 'A' + 10) << 8;
            } else if ('a' <= c2 && c2 <= 'f') {
                *dst |= (ep_u16)(c2 - 'a' + 10) << 8;
            } else return false;

            if ('0' <= c3 && c3 <= '9') {
                *dst |= (ep_u16)(c3 - '0') << 4;
            } else if ('A' <= c3 && c3 <= 'F') {
                *dst |= (ep_u16)(c3 - 'A' + 10) << 4;
            } else if ('a' <= c3 && c3 <= 'f') {
                *dst |= (ep_u16)(c3 - 'a' + 10) << 4;
            } else return false;

            if ('0' <= c4 && c4 <= '9') {
                *dst |= (ep_u16)(c4 - '0');
            } else if ('A' <= c4 && c4 <= 'F') {
                *dst |= (ep_u16)(c4 - 'A' + 10);
            } else if ('a' <= c4 && c4 <= 'f') {
                *dst |= (ep_u16)(c4 - 'a' + 10);
            } else return false;

            return true;
        }
        
        std::string getNextToString(ep_u64 len) {
            return std::string(str.substr(pos, len));
        }

        char getNextChar() {
            return pos < str.size() ? str[pos++] : '\0';
        }
    };

    static std::pair<bool, std::string> Parse(JsonNode* dst, StringReader& reader) {
        /* !docs
        Parse a JSON string into a JsonNode.
        The result is a pair of a boolean indicating success and a string containing an error message if failed.
        */

        #define FAILED(err, msg) \
            { \
                return { false, std::string(err) + ": " + msg + " " + reader.generatePositionString() }; \
            }
        
        reader.eatWhitespace();

        if (reader.nextIs('"')) {
            reader.pos++;
            std::string str;
            str.reserve(64);
            bool isInBackslash = false;

            while (!reader.eof()) {
                if (reader.nextIs('"') && !isInBackslash) {
                    reader.pos++;
                    str.shrink_to_fit();
                    *dst = MakeStringMove(std::move(str));
                    return { true, "" };
                } else if (reader.nextIs('\\') && !isInBackslash) {
                    isInBackslash = true;
                    reader.pos++;
                } else if (isInBackslash) {
                    if (reader.nextIsAny("\"\\/")) {
                        str += reader.getNextChar();
                    } else if (reader.nextIs('b')) {
                        str += '\b';
                        reader.pos++;
                    } else if (reader.nextIs('f')) {
                        str += '\f';
                        reader.pos++;
                    } else if (reader.nextIs('n')) {
                        str += '\n';
                        reader.pos++;
                    } else if (reader.nextIs('r')) {
                        str += '\r';
                        reader.pos++;
                    } else if (reader.nextIs('t')) {
                        str += '\t';
                        reader.pos++;
                    } else if (reader.nextIs('u')) {
                        reader.pos++;

                        ep_u16 u1;
                        if (!reader.readUnicodeEscape(&u1)) FAILED("invalid unicode escape", reader.getNextCharToString());

                        ep_u16 u2 = 0;
                        if (u1 >= 0xD800 && u1 <= 0xDBFF) {
                            if (!reader.nextIsSub("\\u")) FAILED("expected \\u for surrogate pair", reader.getNextCharToString());
                            reader.pos += 2;

                            if (!reader.readUnicodeEscape(&u2)) FAILED("invalid unicode escape", reader.getNextCharToString());
                            if (u2 < 0xDC00 || u2 > 0xDFFF) FAILED("invalid surrogate pair", reader.getNextCharToString());
                        } else if (u1 >= 0xDC00 && u1 <= 0xDFFF) {
                            FAILED("invalid low surrogate", reader.getNextCharToString());
                        }

                        str += toUtfChar(u1, u2);
                    } else {
                        FAILED("unexpected char after backslash", reader.getNextCharToString());
                    }

                    isInBackslash = false;
                } else {
                    str += reader.getNextChar();
                }
            }

            FAILED("unexpected eof", "");
        } else if (reader.nextIsAny("0123456789-")) {
            ep_f64 num = 0;
            bool isNegative = reader.nextIs('-');
            if (isNegative) reader.pos++;

            bool afterDot = false;
            ep_u64 decimal = 1;
            ep_f64 fraction = 0;
            bool hasFraction = false;

            while (!reader.eof()) {
                ep_u8 c = reader.getNextChar();

                if ('0' <= c && c <= '9') {
                    if (!afterDot) {
                        num *= 10;
                        num += (ep_f64)(c - '0');
                    } else {
                        fraction = fraction * 10 + (ep_f64)(c - '0');
                        decimal *= 10;
                        hasFraction = true;
                    }
                } else if (c == '.') {
                    if (afterDot) FAILED("unexpected dot", reader.getNextCharToString());
                    afterDot = true;
                } else if (c == 'e' || c == 'E') {
                    if (hasFraction) num += fraction / (ep_f64)decimal;
                    
                    bool isNegativeExp = reader.nextIs('-');
                    if (isNegativeExp) reader.pos++;
                    else if (reader.nextIs('+')) reader.pos++;

                    ep_u64 exp = 0;
                    bool hasExp = false;
                    while (!reader.eof()) {
                        ep_u8 c = reader.getNextChar();

                        if ('0' <= c && c <= '9') {
                            exp *= 10;
                            exp += (ep_u64)(c - '0');
                            hasExp = true;
                        } else {
                            reader.pos--;
                            break;
                        }
                    }
                    
                    if (!hasExp) FAILED("expected exponent digits", reader.getNextCharToString());

                    if (isNegativeExp) num /= std::pow<ep_f64>(10, exp);
                    else num *= std::pow<ep_f64>(10, exp);
                    *dst = MakeNumber(num * (isNegative ? -1 : 1));
                    return { true, "" };
                } else {
                    reader.pos--;
                    if (hasFraction) num += fraction / (ep_f64)decimal;
                    *dst = MakeNumber(num * (isNegative ? -1 : 1));
                    return { true, "" };
                }
            }
            
            if (hasFraction) num += fraction / (ep_f64)decimal;
            *dst = MakeNumber(num * (isNegative ? -1 : 1));
            return { true, "" };
        } else if (reader.nextIsSubAny({ "true", "false" })) {
            bool b = reader.nextIsSub("true");
            *dst = MakeBool(b);
            reader.pos += b ? 4 : 5;
            return { true, "" };
        } else if (reader.nextIs('[')) {
            reader.pos++;
            std::vector<JsonNode> arr;
            arr.reserve(8);

            while (!reader.eof()) {
                reader.eatWhitespace();

                if (reader.nextIs(']')) {
                    *dst = MakeArrayMove(std::move(arr));
                    reader.pos++;
                    return { true, "" };
                }

                if (arr.size()) {
                    if (!reader.nextIs(',')) FAILED("expected comma", reader.getNextCharToString());
                    reader.pos++;
                    reader.eatWhitespace();
                }

                JsonNode node;
                auto [success, err] = Parse(&node, reader);
                if (!success) return { false, err };

                arr.push_back(std::move(node));
            }

            FAILED("unexpected eof", "");
        } else if (reader.nextIs('{')) {
            reader.pos++;
            std::unordered_map<std::string, JsonNode> obj;
            obj.reserve(8);

            while (!reader.eof()) {
                reader.eatWhitespace();

                if (reader.nextIs('}')) {
                    *dst = MakeObjectMove(std::move(obj));
                    reader.pos++;
                    return { true, "" };
                }

                if (obj.size()) {
                    if (!reader.nextIs(',')) FAILED("expected comma", reader.getNextCharToString());
                    reader.pos++;
                    reader.eatWhitespace();
                }

                JsonNode key;
                {
                    auto [success, err] = Parse(&key, reader);
                    if (!success) return { false, err };
                }

                if (!key.isString()) FAILED("expected string", key.getString());

                reader.eatWhitespace();
                if (!reader.nextIs(':')) FAILED("expected colon", reader.getNextCharToString());
                reader.pos++;
                reader.eatWhitespace();

                JsonNode value;
                {
                    auto [success, err] = Parse(&value, reader);
                    if (!success) return { false, err };
                }

                obj.emplace(std::move(key.getString()), std::move(value));
            }

            FAILED("unexpected eof", "");
        } else if (reader.nextIsSub("null")) {
            *dst = MakeNull();
            reader.pos += 4;
            return { true, "" };
        }

        FAILED("unexpected char", reader.getNextCharToString());
        #undef FAILED
    }

    static std::pair<bool, std::string> Parse(JsonNode* dst, const Data& data) {
        StringReader reader(std::string_view(
            (const char*)data.data.data(),
            data.data.size()
        ));
        return Parse(dst, reader);
    }

    template<typename T>
    void print(T& stream) const {
        /* !docs
        Prints the json node to the given stream.
        */

        if (isString()) {
            stream << '"';
            for (ep_u8 c : getString()) {
                if (c == '"') stream << "\\\"";
                else if (c == '\\') stream << "\\\\";
                else if (c == '\n') stream << "\\n";
                else if (c == '\r') stream << "\\r";
                else if (c == '\t') stream << "\\t";
                else if (c == '\b') stream << "\\b";
                else if (c == '\f') stream << "\\f";
                else stream << c;
            }
            stream << '"';
        } else if (isNumber()) {
            auto number = getNumber();
            stream << (std::fmod(number, 1.0) != 0.0 ? formatToStdString("%.10g", number) : std::to_string((ep_i64)number));
        } else if (isBool()) stream << (getBool() ? "true" : "false");
        else if (isArray()) {
            stream << '[';
            for (ep_u64 i = 0; i < getArray().size(); i++) {
                if (i) stream << ',';
                getArray()[i].print(stream);
            }
            stream << ']';
        } else if (isObject()) {
            stream << '{';
            ep_u64 i = 0;
            for (auto& [key, value] : getObject()) {
                JsonNode::MakeString(key).print(stream);
                stream << ':';
                value.print(stream);
                if (i < getObject().size() - 1) stream << ',';
                i++;
            }
            stream << '}';
        } else if (isNull()) stream << "null";
    }

    void print() const {
        /* !docs
        Prints the json node to the standard output.
        */

        print(std::cout);
    }

    std::string toString() const {
        /* !docs
        Converts the json node to a string.
        */

        std::string result;
        result.reserve(256);
        toStringImpl(result);
        return result;
    }
    
    bool operator==(const JsonNode& other) const {
        if (type != other.type) return false;
        if (type == EnumType::Null) return true;
        if (type == EnumType::String) return getString() == other.getString();
        if (type == EnumType::Number) return getNumber() == other.getNumber();
        if (type == EnumType::Bool) return getBool() == other.getBool();
        if (type == EnumType::Array) {
            if (getArray().size() != other.getArray().size()) return false;
            for (ep_u32 i = 0; i < getArray().size(); i++) {
                if (getArray()[i] != other.getArray()[i]) return false;
            }
            return true;
        }
        if (type == EnumType::Object) {
            if (getObject().size() != other.getObject().size()) return false;
            for (auto& [key, value] : getObject()) {
                if (value != other[key]) return false;
            }
            return true;
        }
        return false;
    }

    bool operator!=(const JsonNode& other) const { return !(*this == other); }

    JsonNode operator[](ep_u64 index) const noexcept {
        return getArray()[index];
    }

    JsonNode operator[](const std::string& key) const noexcept {
        auto it = getObject().find(key);
        if (it != getObject().end()) return it->second;
        return MakeNull();
    }

    JsonNode& operator[](ep_u64 index) noexcept {
        auto& arr = getArray();
        return arr[index];
    }

    JsonNode& operator[](const std::string& key) noexcept {
        auto& obj = getObject();
        return obj[key];
    }

    bool hasKey(const std::string& key) const {
        /* !docs
        Checks if the json node is an object and contains the specified key.
        */

        if (type != EnumType::Object) return false;
        return getObject().contains(key);
    }

private:
    void toStringImpl(std::string& out) const {
        if (isString()) {
            out += '"';
            for (ep_u8 c : getString()) {
                if (c == '"') out += "\\\"";
                else if (c == '\\') out += "\\\\";
                else if (c == '\n') out += "\\n";
                else if (c == '\r') out += "\\r";
                else if (c == '\t') out += "\\t";
                else if (c == '\b') out += "\\b";
                else if (c == '\f') out += "\\f";
                else out += c;
            }
            out += '"';
        } else if (isNumber()) out += formatToStdString("%.10g", getNumber());
        else if (isBool()) out += (getBool() ? "true" : "false");
        else if (isArray()) {
            out += '[';
            for (ep_u64 i = 0; i < getArray().size(); i++) {
                if (i) out += ',';
                getArray()[i].toStringImpl(out);
            }
            out += ']';
        } else if (isObject()) {
            out += '{';
            ep_u64 i = 0;
            for (auto& [key, value] : getObject()) {
                out += '"';
                out += key;
                out += "\":";
                value.toStringImpl(out);
                if (i < getObject().size() - 1) out += ',';
                i++;
            }
            out += '}';
        } else if (isNull()) out += "null";
    }
};

struct DecodedRGBATexture {
    /* !docs
    The decoded RGBA texture.
    The data is a flat array of RGBA pixels.
    */

    std::vector<ep_u8> data;
    ep_u64 width, height;

    static DecodedRGBATexture Make(ep_u64 width, ep_u64 height, ep_u8 init = 0) {
        /* !docs
        Create a new texture with the given width and height, and fill it with the given value.
        */

        return {
            .data = std::vector<ep_u8>(width * height * 4, init),
            .width = width, .height = height
        };
    }

    ep_u64 getIndexBase(ep_u64 x, ep_u64 y) const noexcept {
        return (y * width + x) * 4;
    }

    bool valid() const {
        return width > 0 && height > 0 && data.size() == (width * height * 4);
    }

    void fillWithGray(const std::vector<ep_u8>& gray) {
        if (gray.size() != width * height) throw std::runtime_error("gray data size mismatch");
        ensureDataSize();

        std::fill(data.begin(), data.end(), 255);
        for (ep_u64 i = 0; i < width * height; ++i) {
            data[i * 4 + 3] = gray[i];
        }
    }

    void fillRGBWhite() {
        /* !docs
        Fill the texture with white color.
        */

        ensureDataSize();
        std::fill(data.begin(), data.end(), 255);
        for (ep_u64 i = 0; i < width * height; ++i) data[i * 4 + 3] = 0;
    }

    void paste(const DecodedRGBATexture& other, ep_i64 x, ep_i64 y) noexcept {
        /* !docs
        Paste the other texture onto this texture at the given position.
        */

        if (x >= (ep_i64)width || y >= (ep_i64)height) return;
        if (x + other.width < 0 || y + other.height < 0) return;

        for (ep_i64 i = 0; i < (ep_i64)other.width; i++) {
            ep_i64 px = i + x;
            if (px < 0) continue;
            if (px >= (ep_i64)width) break;

            for (ep_i64 j = 0; j < (ep_i64)other.height; j++) {
                ep_i64 py = j + y;
                if (py < 0) continue;
                if (py >= (ep_i64)height) break;

                auto src_idx = (j * other.width + i) * 4;
                auto dst_idx = (py * width + px) * 4;

                ep_f64 src_a = other.data[src_idx + 3] / 255.0;
                ep_f64 dst_a = data[dst_idx + 3] / 255.0;

                auto a = src_a + dst_a * (1 - src_a);
                data[dst_idx + 3] = (ep_u8)(a * 255);
                if (data[dst_idx + 3] == 0) continue;

                for (ep_i64 k = 0; k < 3; k++) {
                    ep_f64 src = other.data[src_idx + k] / 255.0;
                    ep_f64 dst = data[dst_idx + k] / 255.0;
                    auto color = (src * src_a + dst * dst_a * (1 - src_a)) / a;
                    data[dst_idx + k] = (ep_u8)(color * 255);
                }
            }
        }
    }

    private:
    void ensureDataSize() {
        data.resize(width * height * 4);
    }
};

struct YUV420Frame {
    /* !docs
    A YUV420 frame.
    The data is stored in a `aligned_vector` with the following layout:
    ```
    y[width * height]
    u[width * height / 4]
    v[width * height / 4]
    ```
    */

    aligned_vector<ep_u8, 16> data;
    ep_u64 width, height;

    void ensureSize() {
        if (data.size() != getDataSize()) {
            data.resize(getDataSize());
        }
    }

    static ep_sp<YUV420Frame> Make(ep_u64 width, ep_u64 height) {
        auto* frame = new YUV420Frame();
        frame->width = width;
        frame->height = height;
        frame->ensureSize();
        return ep_sp<YUV420Frame>(frame);
    }

    ep_sp<YUV420Frame> move() {
        auto* frame = new YUV420Frame();
        frame->data = std::move(data);
        frame->width = width;
        frame->height = height;
        return ep_sp<YUV420Frame>(frame);
    }

    ep_u64 getDataSize() const { return width * height * 3 / 2; }

    ep_u8* y() const { return (ep_u8*)data.data(); }
    ep_u8* u() const { return (ep_u8*)data.data() + width * height; }
    ep_u8* v() const { return (ep_u8*)data.data() + width * height + width * height / 4; }
    ep_u64 rowBytesY() const { return width; }
    ep_u64 rowBytesU() const { return width / 2; }
    ep_u64 rowBytesV() const { return width / 2; }

    void fromPtr(void* ptr) {
        /* !docs
        Fills the frame with data from a pointer.
        */

        memcpy(data.data(), ptr, getDataSize());
    }
};

struct Vec2 {
    ep_f64 x, y;

    Vec2() = default;
    template <typename A, typename B> Vec2(A a, B b) : x((ep_f64)a), y((ep_f64)b) {}

    ep_f64 sum() const noexcept { return x + y; }
    ep_f64 length() const noexcept { return std::sqrt(x * x + y * y); }
    ep_f64 lengthSquared() const noexcept { return x * x + y * y; }
    ep_f64 xyDiff() const noexcept { return std::abs(x - y); }
    ep_f64 max() const noexcept { return std::max(x, y); }
    ep_f64 min() const noexcept { return std::min(x, y); }

    Vec2 operator+(const Vec2& v) const noexcept { return Vec2 { x + v.x, y + v.y }; }
    Vec2 operator-(const Vec2& v) const noexcept { return Vec2 { x - v.x, y - v.y }; }
    Vec2 operator*(const Vec2& v) const noexcept { return Vec2 { x * v.x, y * v.y }; }
    Vec2 operator/(const Vec2& v) const noexcept { return Vec2 { x / v.x, y / v.y }; }
    Vec2 operator+(ep_f64 v) const noexcept { return Vec2 { x + v, y + v }; }
    Vec2 operator-(ep_f64 v) const noexcept { return Vec2 { x - v, y - v }; }
    Vec2 operator*(ep_f64 v) const noexcept { return Vec2 { x * v, y * v }; }
    Vec2 operator/(ep_f64 v) const noexcept { return Vec2 { x / v, y / v }; }
    Vec2 operator-() const noexcept { return Vec2 { -x, -y }; }

    Vec2& operator+=(const Vec2& v) noexcept { x += v.x; y += v.y; return *this; }
    Vec2& operator-=(const Vec2& v) noexcept { x -= v.x; y -= v.y; return *this; }
    Vec2& operator*=(const Vec2& v) noexcept { x *= v.x; y *= v.y; return *this; }
    Vec2& operator/=(const Vec2& v) noexcept { x /= v.x; y /= v.y; return *this; }
    Vec2& operator+=(ep_f64 v) noexcept { x += v; y += v; return *this; }
    Vec2& operator-=(ep_f64 v) noexcept { x -= v; y -= v; return *this; }
    Vec2& operator*=(ep_f64 v) noexcept { x *= v; y *= v; return *this; }
    Vec2& operator/=(ep_f64 v) noexcept { x /= v; y /= v; return *this; }

    bool operator==(const Vec2& v) const noexcept { return x == v.x && y == v.y; }
    bool operator!=(const Vec2& v) const noexcept { return x != v.x || y != v.y; }

    Vec2 rotate(ep_f64 angle, ep_f64 length) const noexcept {
        ep_f64 c = std::cos(angle); ep_f64 s = std::sin(angle);
        return Vec2 { x + c * length, y + s * length };
    }

    Vec2 rotateDegrees(ep_f64 angle, ep_f64 length) const noexcept { return rotate(angle / 180.0 * std::numbers::pi, length); }
    bool isZeroZone() const noexcept { return x == y; }
    bool include(ep_f64 v) const noexcept { return x <= v && v <= y; }
    std::pair<ep_f64, ep_f64> toPair() const noexcept { return { x, y }; }
};

struct Rect {
    ep_f64 x, y, w, h;

    static Rect MakeCenter(ep_f64 x, ep_f64 y, ep_f64 w, ep_f64 h) noexcept {
        return { x - w / 2, y - h / 2, w, h };
    }

    Vec2 position() const noexcept { return { x, y }; }
    Vec2 size() const noexcept { return { w, h }; }
    Vec2 center() const noexcept { return { x + w / 2, y + h / 2 }; }

    Rect extend(ep_f64 padding) const noexcept {
        /* !docs
        Returns a new rect with the padding applied to all sides.
        */

        return Rect {
            .x = x - padding,
            .y = y - padding,
            .w = w + padding * 2,
            .h = h + padding * 2
        };
    }
};

struct Color {
    ep_f64 r, g, b, a;

    static Color White() noexcept { return Color { 1.0, 1.0, 1.0, 1.0 }; }
    static Color Black() noexcept { return Color { 0.0, 0.0, 0.0, 1.0 }; }
    static Color Red() noexcept { return Color { 1.0, 0.0, 0.0, 1.0 }; }
    static Color Green() noexcept { return Color { 0.0, 1.0, 0.0, 1.0 }; }
    static Color Blue() noexcept { return Color { 0.0, 0.0, 1.0, 1.0 }; }
    static Color Transparent() noexcept { return Color { 0.0, 0.0, 0.0, 0.0 }; }

    Color applyAlpha(ep_f64 alpha) const noexcept {
        /* !docs
        Returns a new color with the alpha multiplied by `alpha`.
        */

        return Color { r, g, b, a * alpha };
    }

    Color operator*(const Color& c) const noexcept { return Color { r * c.r, g * c.g, b * c.b, a * c.a }; }
    Color operator*(ep_f64 v) const noexcept { return Color { r * v, g * v, b * v, a * v }; }
    Color operator+(const Color& c) const noexcept { return Color { r + c.r, g + c.g, b + c.b, a + c.a }; }
    Color operator+(ep_f64 v) const noexcept { return Color { r + v, g + v, b + v, a + v }; }
    Color operator-(const Color& c) const noexcept { return Color { r - c.r, g - c.g, b - c.b, a - c.a }; }
    Color operator-(ep_f64 v) const noexcept { return Color { r - v, g - v, b - v, a - v }; }
    Color operator/(const Color& c) const noexcept { return Color { r / c.r, g / c.g, b / c.b, a / c.a }; }
    Color operator/(ep_f64 v) const noexcept { return Color { r / v, g / v, b / v, a / v }; }

    Color& operator*=(const Color& c) noexcept { r *= c.r; g *= c.g; b *= c.b; a *= c.a; return *this; }
    Color& operator*=(ep_f64 v) noexcept { r *= v; g *= v; b *= v; a *= v; return *this; }
    Color& operator+=(const Color& c) noexcept { r += c.r; g += c.g; b += c.b; a += c.a; return *this; }
    Color& operator+=(ep_f64 v) noexcept { r += v; g += v; b += v; a += v; return *this; }
    Color& operator-=(const Color& c) noexcept { r -= c.r; g -= c.g; b -= c.b; a -= c.a; return *this; }
    Color& operator-=(ep_f64 v) noexcept { r -= v; g -= v; b -= v; a -= v; return *this; }
    Color& operator/=(const Color& c) noexcept { r /= c.r; g /= c.g; b /= c.b; a /= c.a; return *this; }
    Color& operator/=(ep_f64 v) noexcept { r /= v; g /= v; b /= v; a /= v; return *this; }

    bool operator==(const Color& c) const noexcept { return r == c.r && g == c.g && b == c.b && a == c.a; }
    bool operator!=(const Color& c) const noexcept { return !(*this == c); }
};

struct ObjectIndexer {
    /* !docs
    A class that stores the index of a object.
    */

    ep_u64 index;

    ep_u64 get() noexcept {
        return index ? index : (index = reqGlobalCounter());
    }

    void set(ep_u64 given) noexcept {
        index = given;
    }
};

template <typename T>
struct ObjectIndexGenerator {
    ep_u64 get(const T& key) noexcept {
        auto it = map.find(key);
        if (it == map.end()) {
            return map[key] = reqGlobalCounter();
        }
        return it->second;
    }

    private:
    std::map<T, ep_u64> map;
};

struct Timer {
    ep_f64 start;

    Timer() : start(globalTimer()) {}

    ep_f64 elapsed() const noexcept { return globalTimer() - start; }
};

struct Transform2D {
    /* !docs
    A 2D transformation by a 3x3 matrix.
    */

    ep_f64 matrix[6];

    Transform2D(ep_f64 a, ep_f64 b, ep_f64 c, ep_f64 d, ep_f64 e, ep_f64 f) noexcept {
        matrix[0] = a; matrix[1] = b;
        matrix[2] = c; matrix[3] = d;
        matrix[4] = e; matrix[5] = f;
    }

    Transform2D() noexcept {
        matrix[0] = 1.0; matrix[1] = 0.0;
        matrix[2] = 0.0; matrix[3] = 1.0;
        matrix[4] = 0.0; matrix[5] = 0.0;
    }

    Transform2D& set(ep_f64 a, ep_f64 b, ep_f64 c, ep_f64 d, ep_f64 e, ep_f64 f) noexcept {
        matrix[0] = a; matrix[1] = b;
        matrix[2] = c; matrix[3] = d;
        matrix[4] = e; matrix[5] = f;
        return *this;
    }

    Transform2D& transform(ep_f64 a, ep_f64 b, ep_f64 c, ep_f64 d, ep_f64 e, ep_f64 f) noexcept {
        set(
            matrix[0] * a + matrix[2] * b,
            matrix[1] * a + matrix[3] * b,
            matrix[0] * c + matrix[2] * d,
            matrix[1] * c + matrix[3] * d,
            matrix[0] * e + matrix[2] * f + matrix[4],
            matrix[1] * e + matrix[3] * f + matrix[5]
        );
        return *this;
    }

    Transform2D& transform(const Transform2D& o) noexcept {
        transform(
            o.matrix[0], o.matrix[1],
            o.matrix[2], o.matrix[3],
            o.matrix[4], o.matrix[5]
        );
        return *this;
    }

    Transform2D& scale(ep_f64 x, ep_f64 y) noexcept {
        transform(x, 0.0, 0.0, y, 0.0, 0.0);
        return *this;
    }

    Transform2D& scale(const Vec2& v) noexcept {
        scale(v.x, v.y);
        return *this;
    }

    Transform2D& scale(ep_f64 s) noexcept {
        scale(s, s);
        return *this;
    }

    Transform2D& translate(ep_f64 x, ep_f64 y) noexcept {
        transform(1.0, 0.0, 0.0, 1.0, x, y);
        return *this;
    }

    Transform2D& translate(const Vec2& v) noexcept {
        translate(v.x, v.y);
        return *this;
    }

    Transform2D& rotate(ep_f64 angle) noexcept {
        ep_f64 c = std::cos(angle);
        ep_f64 s = std::sin(angle);
        transform(c, s, -s, c, 0.0, 0.0);
        return *this;
    }

    Transform2D& rotateDegrees(ep_f64 angle) noexcept {
        rotate(angle / 180.0 * std::numbers::pi);
        return *this;
    }

    Vec2 transformPoint(ep_f64 x, ep_f64 y) const noexcept {
        return Vec2 {
            matrix[0] * x + matrix[2] * y + matrix[4],
            matrix[1] * x + matrix[3] * y + matrix[5]
        };
    }

    Vec2 transformPoint(const Vec2& v) const noexcept {
        return transformPoint(v.x, v.y);
    }

    Transform2D getInverse() const noexcept {
        ep_f64 det = matrix[0] * matrix[3] - matrix[1] * matrix[2];
        ep_f64 invDet = det != 0 ? 1.0 / det : 1e9;
        return Transform2D(
            matrix[3] * invDet, -matrix[1] * invDet,
            -matrix[2] * invDet, matrix[0] * invDet,
            (matrix[2] * matrix[5] - matrix[3] * matrix[4]) * invDet,
            (matrix[1] * matrix[4] - matrix[0] * matrix[5]) * invDet
        );
    }
};

bool pointStrictlyInConvexQuad(const Vec2& p, const Vec2 quad[4]) noexcept {
    /* !docs
    Checks if a point is strictly inside a convex quad.
    */

    auto cross = [](ep_f64 ax, ep_f64 ay, ep_f64 bx, ep_f64 by) {
        return ax * by - ay * bx;
    };

    auto x = p.x, y = p.y;
    auto cross0 = cross(quad[1].x - quad[0].x, quad[1].y - quad[0].y, x - quad[0].x, y - quad[0].y);
    auto cross1 = cross(quad[2].x - quad[1].x, quad[2].y - quad[1].y, x - quad[1].x, y - quad[1].y);
    auto cross2 = cross(quad[3].x - quad[2].x, quad[3].y - quad[2].y, x - quad[2].x, y - quad[2].y);
    auto cross3 = cross(quad[0].x - quad[3].x, quad[0].y - quad[3].y, x - quad[3].x, y - quad[3].y);
    
    if (cross0 < 0 && cross1 < 0 && cross2 < 0 && cross3 < 0) return true;
    if (cross0 > 0 && cross1 > 0 && cross2 > 0 && cross3 > 0) return true;
    
    return false;
}

bool pointStrictlyInRect(const Vec2& p, const Rect& r) noexcept {
    /* !docs
    Checks if a point is strictly inside a rectangle.
    */

    return r.x < p.x && p.x < r.x + r.w &&
           r.y < p.y && p.y < r.y + r.h;
}

bool quadStrictlyIntersectRect(const Vec2 quad[4], const Rect& r) noexcept {
    /* !docs
    Checks if a convex quad is strictly intersecting a rectangle.
    */

    return pointStrictlyInRect(quad[0], r) ||
           pointStrictlyInRect(quad[1], r) ||
           pointStrictlyInRect(quad[2], r) ||
           pointStrictlyInRect(quad[3], r) ||
           pointStrictlyInConvexQuad(Vec2 {r.x, r.y}, quad) ||
           pointStrictlyInConvexQuad(Vec2 {r.x + r.w, r.y}, quad) ||
           pointStrictlyInConvexQuad(Vec2 {r.x + r.w, r.y + r.h}, quad) ||
           pointStrictlyInConvexQuad(Vec2 {r.x, r.y + r.h}, quad);
}

bool lineIsIntersectLineSeg(const Vec2& linePoint, ep_f64 lineDeg, const Vec2 seg[2]) noexcept {
    /* !docs
    Checks if a **line** is intersecting a **line segment**.
    */

    ep_f64 angle = lineDeg / 180.0 * std::numbers::pi;
    Vec2 dir = { std::cos(angle), std::sin(angle) };
    
    Vec2 s = seg[1] - seg[0];
    Vec2 q = seg[0] - linePoint;
    
    ep_f64 rxs = dir.x * s.y - dir.y * s.x;
    ep_f64 qxs = q.x * s.y - q.y * s.x;
    
    constexpr ep_f64 eps = 1e-9;
    
    if (std::abs(rxs) < eps) {
        if (std::abs(qxs) >= eps) return false;
        return true;
    }
    
    ep_f64 u = (q.x * dir.y - q.y * dir.x) / rxs;
    return u >= -eps && u <= 1.0 + eps;
}

bool lineIsIntersectRect(const Vec2& linePoint, ep_f64 lineDeg, const Rect& r) noexcept {
    /* !docs
    Checks if a line is intersecting a rectangle.
    */

    return lineIsIntersectLineSeg(linePoint, lineDeg, (Vec2[2]) { Vec2 { r.x, r.y }, Vec2 { r.x + r.w, r.y } }) ||
           lineIsIntersectLineSeg(linePoint, lineDeg, (Vec2[2]) { Vec2 { r.x + r.w, r.y }, Vec2 { r.x + r.w, r.y + r.h } }) ||
           lineIsIntersectLineSeg(linePoint, lineDeg, (Vec2[2]) { Vec2 { r.x + r.w, r.y + r.h }, Vec2 { r.x, r.y + r.h } }) ||
           lineIsIntersectLineSeg(linePoint, lineDeg, (Vec2[2]) { Vec2 { r.x, r.y + r.h }, Vec2 { r.x, r.y } });
}

bool pointIsLeavingPoint(const Vec2& point, ep_f64 deg, const Vec2& targetPoint) noexcept {
    /* !docs
    Checks if a point is leaving a target point.
    When it returns true, it means that the point is leaving the target point if it is moving in the given direction.
    */

    ep_f64 eps = 1.0;
    return (
        (point.rotateDegrees(deg + 90, -eps) - targetPoint).lengthSquared() -
        (point - targetPoint).lengthSquared()
    ) > 0;
}

bool lineIsLeavingScreen(const Vec2& linePoint, ep_f64 lineDeg, const Rect& screenArea) noexcept {
    /* !docs
    Checks if a line is leaving the screen based on `@pointIsLeavingPoint`.
    */

    return !lineIsIntersectRect(linePoint, lineDeg, screenArea) && pointIsLeavingPoint(linePoint, lineDeg, screenArea.center());
}

Rect getCoveredOrContainRect(const Rect& dst, const Vec2& size, bool isCovered) {
    ep_f64 dst_ratio = dst.w / dst.h;
    ep_f64 src_ratio = size.x / size.y;

    ep_f64 w, h;
    if (isCovered ? (src_ratio < dst_ratio) : (src_ratio > dst_ratio)) {
        w = dst.w;
        h = dst.w / src_ratio;
    } else {
        w = dst.h * src_ratio;
        h = dst.h;
    }

    return Rect::MakeCenter(dst.x + dst.w / 2, dst.y + dst.h / 2, w, h);
}

void stripString(std::string& str) {
    /* !docs
    Strip a string like python's `str.strip()`.
    */

    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    auto tail = std::ranges::find_if(str | std::views::reverse, not_space);
    str.erase(tail.base(), str.end());
    auto head = std::ranges::find_if(str, not_space);
    str.erase(str.begin(), head);
}

void splitString(const std::string& str, std::vector<std::string>& lines, char delimiter = '\n') {
    /* !docs
    Split a string to lines like python's `str.split(delimiter)`.
    */

    for (auto&& subrange : str | std::views::split(delimiter)) {
        lines.emplace_back(subrange.begin(), subrange.end());
    }
}

bool stringIsStartsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
}

std::string replaceStringWith(const std::string& str, const std::string& target, const std::string& replacement) {
    if (target.empty()) return str;

    std::string result;
    size_t start = 0;
    size_t pos;
    while ((pos = str.find(target, start)) != std::string::npos) {
        result.append(str, start, pos - start);
        result.append(replacement);
        start = pos + target.size();
    }
    result.append(str, start, std::string::npos);
    return result;
}

std::string stringSliceProgress(const std::string& str, ep_f64 p) {
    p = std::clamp(p, 0.0, 1.0);
    return str.substr(0, (ep_u64)(str.size() * p));
}

struct EaseSet {
    /* !docs
    A set of easing functions.
    */

    static constexpr ep_u64 ktIntegralTableSize = 128;

    static ep_f64 getIntegralValue(const ep_f64* table, ep_u64 size, ep_f64 p) noexcept {
        p = std::clamp(p, 0.0, 1.0);
        if (p == 1.0) return table[size - 1];
        auto s = table[(ep_u64)(p * (size - 1))];
        auto e = table[(ep_u64)((p * (size - 1)) + 1)];
        p = std::fmod(p, 1.0 / (size - 1)) * (size - 1);
        return s + (e - s) * p;
    }

    struct Milthm {
        static ep_f64 easing_in(ep_u64 press, ep_f64 p) noexcept {
            switch (press) {
                case 0: return p;
                case 1: return (1.0 - cos(((p * std::numbers::pi) / 2.0)));
                case 2: return pow(p, 2.0);
                case 3: return pow(p, 3.0);
                case 4: return pow(p, 4.0);
                case 5: return pow(p, 5.0);
                case 6: return ((p == 0.0) ? 0.0 : pow(2.0, ((10.0 * p) - 10.0)));
                case 7: return (1.0 - pow((1.0 - pow(p, 2.0)), 0.5));
                case 8: return ((2.70158 * pow(p, 3.0)) - (1.70158 * pow(p, 2.0)));
                case 9: return ((p == 0.0) ? 0.0 : ((p == 1.0) ? 1.0 : ((-pow(2.0, ((10.0 * p) - 10.0))) * sin((((p * 10.0) - 10.75) * ((2.0 * std::numbers::pi) / 3.0))))));
                case 10: return (1.0 - (((1.0 - p) < (1.0 / 2.75)) ? (7.5625 * pow((1.0 - p), 2.0)) : (((1.0 - p) < (2.0 / 2.75)) ? (((7.5625 * ((1.0 - p) - (1.5 / 2.75))) * ((1.0 - p) - (1.5 / 2.75))) + 0.75) : (((1.0 - p) < (2.5 / 2.75)) ? (((7.5625 * ((1.0 - p) - (2.25 / 2.75))) * ((1.0 - p) - (2.25 / 2.75))) + 0.9375) : (((7.5625 * ((1.0 - p) - (2.625 / 2.75))) * ((1.0 - p) - (2.625 / 2.75))) + 0.984375)))));
                default: return p;
            }
        }

        static ep_f64 easing_out(ep_u64 press, ep_f64 p) noexcept {
            switch (press) {
                case 0: return p;
                case 1: return sin(((p * std::numbers::pi) / 2.0));
                case 2: return (1.0 - ((1.0 - p) * (1.0 - p)));
                case 3: return (1.0 - pow((1.0 - p), 3.0));
                case 4: return (1.0 - pow((1.0 - p), 4.0));
                case 5: return (1.0 - pow((1.0 - p), 5.0));
                case 6: return ((p == 1.0) ? 1.0 : (1.0 - pow(2.0, ((-10.0) * p))));
                case 7: return pow((1.0 - pow((p - 1.0), 2.0)), 0.5);
                case 8: return ((1.0 + (2.70158 * pow((p - 1.0), 3.0))) + (1.70158 * pow((p - 1.0), 2.0)));
                case 9: return ((p == 0.0) ? 0.0 : ((p == 1.0) ? 1.0 : ((pow(2.0, ((-10.0) * p)) * sin((((p * 10.0) - 0.75) * ((2.0 * std::numbers::pi) / 3.0)))) + 1.0)));
                case 10: return ((p < (1.0 / 2.75)) ? (7.5625 * pow(p, 2.0)) : ((p < (2.0 / 2.75)) ? (((7.5625 * (p - (1.5 / 2.75))) * (p - (1.5 / 2.75))) + 0.75) : ((p < (2.5 / 2.75)) ? (((7.5625 * (p - (2.25 / 2.75))) * (p - (2.25 / 2.75))) + 0.9375) : (((7.5625 * (p - (2.625 / 2.75))) * (p - (2.625 / 2.75))) + 0.984375))));
                default: return p;
            }
        }

        static ep_f64 easing_in_out(ep_u64 press, ep_f64 p) noexcept {
            switch (press) {
                case 0: return p;
                case 1: return ((-(cos((std::numbers::pi * p)) - 1.0)) / 2.0);
                case 2: return ((p < 0.5) ? (2.0 * pow(p, 2.0)) : (1.0 - (pow((((-2.0) * p) + 2.0), 2.0) / 2.0)));
                case 3: return ((p < 0.5) ? (4.0 * pow(p, 3.0)) : (1.0 - (pow((((-2.0) * p) + 2.0), 3.0) / 2.0)));
                case 4: return ((p < 0.5) ? (8.0 * pow(p, 4.0)) : (1.0 - (pow((((-2.0) * p) + 2.0), 4.0) / 2.0)));
                case 5: return ((p < 0.5) ? (16.0 * pow(p, 5.0)) : (1.0 - (pow((((-2.0) * p) + 2.0), 5.0) / 2.0)));
                case 6: return ((p == 0.0) ? 0.0 : ((p == 1.0) ? 1.0 : (((p < 0.5) ? pow(2.0, ((20.0 * p) - 10.0)) : (2.0 - pow(2.0, (((-20.0) * p) + 10.0)))) / 2.0)));
                case 7: return ((p < 0.5) ? ((1.0 - pow((1.0 - pow((2.0 * p), 2.0)), 0.5)) / 2.0) : ((pow((1.0 - pow((((-2.0) * p) + 2.0), 2.0)), 0.5) + 1.0) / 2.0));
                case 8: return ((p < 0.5) ? ((pow((2.0 * p), 2.0) * ((((2.5949095 + 1.0) * 2.0) * p) - 2.5949095)) / 2.0) : (((pow(((2.0 * p) - 2.0), 2.0) * (((2.5949095 + 1.0) * ((p * 2.0) - 2.0)) + 2.5949095)) + 2.0) / 2.0));
                case 9: return ((p == 0.0) ? 0.0 : ((p == 0.0) ? 1.0 : ((p < 0.5) ? (((-pow(2.0, ((20.0 * p) - 10.0))) * sin((((20.0 * p) - 11.125) * ((2.0 * std::numbers::pi) / 4.5)))) / 2.0) : (((pow(2.0, (((-20.0) * p) + 10.0)) * sin((((20.0 * p) - 11.125) * ((2.0 * std::numbers::pi) / 4.5)))) / 2.0) + 1.0))));
                case 10: return ((p < 0.5) ? ((1.0 - (((1.0 - (2.0 * p)) < (1.0 / 2.75)) ? (7.5625 * pow((1.0 - (2.0 * p)), 2.0)) : (((1.0 - (2.0 * p)) < (2.0 / 2.75)) ? (((7.5625 * ((1.0 - (2.0 * p)) - (1.5 / 2.75))) * ((1.0 - (2.0 * p)) - (1.5 / 2.75))) + 0.75) : (((1.0 - (2.0 * p)) < (2.5 / 2.75)) ? (((7.5625 * ((1.0 - (2.0 * p)) - (2.25 / 2.75))) * ((1.0 - (2.0 * p)) - (2.25 / 2.75))) + 0.9375) : (((7.5625 * ((1.0 - (2.0 * p)) - (2.625 / 2.75))) * ((1.0 - (2.0 * p)) - (2.625 / 2.75))) + 0.984375))))) / 2.0) : ((1.0 + ((((2.0 * p) - 1.0) < (1.0 / 2.75)) ? (7.5625 * pow(((2.0 * p) - 1.0), 2.0)) : ((((2.0 * p) - 1.0) < (2.0 / 2.75)) ? (((7.5625 * (((2.0 * p) - 1.0) - (1.5 / 2.75))) * (((2.0 * p) - 1.0) - (1.5 / 2.75))) + 0.75) : ((((2.0 * p) - 1.0) < (2.5 / 2.75)) ? (((7.5625 * (((2.0 * p) - 1.0) - (2.25 / 2.75))) * (((2.0 * p) - 1.0) - (2.25 / 2.75))) + 0.9375) : (((7.5625 * (((2.0 * p) - 1.0) - (2.625 / 2.75))) * (((2.0 * p) - 1.0) - (2.625 / 2.75))) + 0.984375))))) / 2.0));
                default: return p;
            }
        }

        static ep_f64 easing(ep_u64 ease, ep_u64 press, ep_f64 p) noexcept {
            switch (ease) {
                case 0: return easing_in(press, p);
                case 1: return easing_out(press, p);
                case 2: return easing_in_out(press, p);
                default: return p;
            }
        }
    };

    struct Phigros {
        struct Official {
            static ep_f64 easing(ep_u64 ease, ep_f64 p) noexcept {
                switch (ease) {
                    case 0: return p;
                    case 1: return 1.0 - cos(p * std::numbers::pi / 2.0);
                    case 2: return sin(p * std::numbers::pi / 2.0);
                    case 3: return (1.0 - cos(p * std::numbers::pi)) / 2.0;
                    case 4: return pow(p, 2.0);
                    case 5: return 1.0 - pow(p - 1.0, 2.0);
                    case 6: return ((p *= 2.0) < 1.0 ? pow(p, 2.0) : (-(pow(p - 2.0, 2.0) - 2.0))) / 2.0;
                    case 7: return pow(p, 3.0);
                    case 8: return 1.0 + pow(p - 1.0, 3.0);
                    case 9: return ((p *= 2.0) < 1.0 ? pow(p, 3.0) : (2.0 * pow(p - 2.0, 3.0) + 2.0)) / 2.0;
                    case 10: return pow(p, 4.0);
                    case 11: return 1.0 - pow(p - 1.0, 4.0);
                    case 12: return ((p *= 2.0) < 1.0 ? pow(p, 4.0) : (-(pow(p - 2.0, 4.0) - 2.0))) / 2.0;
                    case 13: return 0.0;
                    case 14: return 1.0;
                    default: return p;
                }
            }
        };

        struct RePhiEdit {
            static ep_f64 easing(ep_u64 ease, ep_f64 p) noexcept {
                switch (ease) {
                    case 1: return p;
                    case 2: return sin(((p * std::numbers::pi) / 2.0));
                    case 3: return (1.0 - cos(((p * std::numbers::pi) / 2.0)));
                    case 4: return (1.0 - ((1.0 - p) * (1.0 - p)));
                    case 5: return pow(p, 2.0);
                    case 6: return ((-(cos((std::numbers::pi * p)) - 1.0)) / 2.0);
                    case 7: return ((p < 0.5) ? (2.0 * pow(p, 2.0)) : (1.0 - (pow((((-2.0) * p) + 2.0), 2.0) / 2.0)));
                    case 8: return (1.0 - pow((1.0 - p), 3.0));
                    case 9: return pow(p, 3.0);
                    case 10: return (1.0 - pow((1.0 - p), 4.0));
                    case 11: return pow(p, 4.0);
                    case 12: return ((p < 0.5) ? (4.0 * pow(p, 3.0)) : (1.0 - (pow((((-2.0) * p) + 2.0), 3.0) / 2.0)));
                    case 13: return ((p < 0.5) ? (8.0 * pow(p, 4.0)) : (1.0 - (pow((((-2.0) * p) + 2.0), 4.0) / 2.0)));
                    case 14: return (1.0 - pow((1.0 - p), 5.0));
                    case 15: return pow(p, 5.0);
                    case 16: return ((p == 1.0) ? 1.0 : (1.0 - pow(2.0, ((-10.0) * p))));
                    case 17: return ((p == 0.0) ? 0.0 : pow(2.0, ((10.0 * p) - 10.0)));
                    case 18: return pow((1.0 - pow((p - 1.0), 2.0)), 0.5);
                    case 19: return (1.0 - pow((1.0 - pow(p, 2.0)), 0.5));
                    case 20: return ((1.0 + (2.70158 * pow((p - 1.0), 3.0))) + (1.70158 * pow((p - 1.0), 2.0)));
                    case 21: return ((2.70158 * pow(p, 3.0)) - (1.70158 * pow(p, 2.0)));
                    case 22: return ((p < 0.5) ? ((1.0 - pow((1.0 - pow((2.0 * p), 2.0)), 0.5)) / 2.0) : ((pow((1.0 - pow((((-2.0) * p) + 2.0), 2.0)), 0.5) + 1.0) / 2.0));
                    case 23: return ((p < 0.5) ? ((pow((2.0 * p), 2.0) * ((((2.5949095 + 1.0) * 2.0) * p) - 2.5949095)) / 2.0) : (((pow(((2.0 * p) - 2.0), 2.0) * (((2.5949095 + 1.0) * ((p * 2.0) - 2.0)) + 2.5949095)) + 2.0) / 2.0));
                    case 24: return ((p == 0.0) ? 0.0 : ((p == 1.0) ? 1.0 : ((pow(2.0, ((-10.0) * p)) * sin((((p * 10.0) - 0.75) * ((2.0 * std::numbers::pi) / 3.0)))) + 1.0)));
                    case 25: return ((p == 0.0) ? 0.0 : ((p == 1.0) ? 1.0 : ((-pow(2.0, ((10.0 * p) - 10.0))) * sin((((p * 10.0) - 10.75) * ((2.0 * std::numbers::pi) / 3.0))))));
                    case 26: return ((p < (1.0 / 2.75)) ? (7.5625 * pow(p, 2.0)) : ((p < (2.0 / 2.75)) ? (((7.5625 * (p - (1.5 / 2.75))) * (p - (1.5 / 2.75))) + 0.75) : ((p < (2.5 / 2.75)) ? (((7.5625 * (p - (2.25 / 2.75))) * (p - (2.25 / 2.75))) + 0.9375) : (((7.5625 * (p - (2.625 / 2.75))) * (p - (2.625 / 2.75))) + 0.984375))));
                    case 27: return (1.0 - (((1.0 - p) < (1.0 / 2.75)) ? (7.5625 * pow((1.0 - p), 2.0)) : (((1.0 - p) < (2.0 / 2.75)) ? (((7.5625 * ((1.0 - p) - (1.5 / 2.75))) * ((1.0 - p) - (1.5 / 2.75))) + 0.75) : (((1.0 - p) < (2.5 / 2.75)) ? (((7.5625 * ((1.0 - p) - (2.25 / 2.75))) * ((1.0 - p) - (2.25 / 2.75))) + 0.9375) : (((7.5625 * ((1.0 - p) - (2.625 / 2.75))) * ((1.0 - p) - (2.625 / 2.75))) + 0.984375)))));
                    case 28: return ((p < 0.5) ? ((1.0 - (((1.0 - (2.0 * p)) < (1.0 / 2.75)) ? (7.5625 * pow((1.0 - (2.0 * p)), 2.0)) : (((1.0 - (2.0 * p)) < (2.0 / 2.75)) ? (((7.5625 * ((1.0 - (2.0 * p)) - (1.5 / 2.75))) * ((1.0 - (2.0 * p)) - (1.5 / 2.75))) + 0.75) : (((1.0 - (2.0 * p)) < (2.5 / 2.75)) ? (((7.5625 * ((1.0 - (2.0 * p)) - (2.25 / 2.75))) * ((1.0 - (2.0 * p)) - (2.25 / 2.75))) + 0.9375) : (((7.5625 * ((1.0 - (2.0 * p)) - (2.625 / 2.75))) * ((1.0 - (2.0 * p)) - (2.625 / 2.75))) + 0.984375))))) / 2.0) : ((1.0 + ((((2.0 * p) - 1.0) < (1.0 / 2.75)) ? (7.5625 * pow(((2.0 * p) - 1.0), 2.0)) : ((((2.0 * p) - 1.0) < (2.0 / 2.75)) ? (((7.5625 * (((2.0 * p) - 1.0) - (1.5 / 2.75))) * (((2.0 * p) - 1.0) - (1.5 / 2.75))) + 0.75) : ((((2.0 * p) - 1.0) < (2.5 / 2.75)) ? (((7.5625 * (((2.0 * p) - 1.0) - (2.25 / 2.75))) * (((2.0 * p) - 1.0) - (2.25 / 2.75))) + 0.9375) : (((7.5625 * (((2.0 * p) - 1.0) - (2.625 / 2.75))) * (((2.0 * p) - 1.0) - (2.625 / 2.75))) + 0.984375))))) / 2.0));
                    case 29: return ((p == 0.0) ? 0.0 : ((p == 1.0) ? 1.0 : ((p < 0.5) ? (((-pow(2.0, ((20.0 * p) - 10.0))) * sin((((20.0 * p) - 11.125) * ((2.0 * std::numbers::pi) / 4.5)))) / 2.0) : (((pow(2.0, (((-20.0) * p) + 10.0)) * sin((((20.0 * p) - 11.125) * ((2.0 * std::numbers::pi) / 4.5)))) / 2.0) + 1.0))));
                    default: return p;
                }
            }

            static constexpr ep_f64 intTable[29][ktIntegralTableSize] = {
                { 0, 3.10002e-05, 0.000124001, 0.000279002, 0.000496003, 0.000775005, 0.00111601, 0.00151901, 0.00198401, 0.00251101, 0.00310002, 0.00375102, 0.00446403, 0.00523903, 0.00607604, 0.00697504, 0.00793605, 0.00895905, 0.0100441, 0.0111911, 0.0124001, 0.0136711, 0.0150041, 0.0163991, 0.0178561, 0.0193751, 0.0209561, 0.0225991, 0.0243041, 0.0260712, 0.0279002, 0.0297912, 0.0317442, 0.0337592, 0.0358362, 0.0379752, 0.0401762, 0.0424392, 0.0447643, 0.0471513, 0.0496003, 0.0521113, 0.0546843, 0.0573193, 0.0600163, 0.0627754, 0.0655964, 0.0684794, 0.0714244, 0.0744314, 0.0775005, 0.0806315, 0.0838245, 0.0870795, 0.0903965, 0.0937755, 0.0972166, 0.10072, 0.104285, 0.107912, 0.111601, 0.115352, 0.119165, 0.12304, 0.126977, 0.130976, 0.135037, 0.13916, 0.143345, 0.147592, 0.151901, 0.156272, 0.160705, 0.1652, 0.169757, 0.174376, 0.179057, 0.1838, 0.188605, 0.193472, 0.198401, 0.203392, 0.208445, 0.21356, 0.218737, 0.223976, 0.229277, 0.23464, 0.240065, 0.245552, 0.251101, 0.256712, 0.262386, 0.268121, 0.273918, 0.279777, 0.285698, 0.291681, 0.297726, 0.303833, 0.310002, 0.316233, 0.322526, 0.328881, 0.335298, 0.341777, 0.348318, 0.354921, 0.361586, 0.368313, 0.375102, 0.381953, 0.388866, 0.395841, 0.402878, 0.409977, 0.417138, 0.424361, 0.431647, 0.438994, 0.446403, 0.453874, 0.461407, 0.469002, 0.476659, 0.484378, 0.492159, 0.500002 },
                { 0, 4.86943e-05, 0.00019477, 0.000438204, 0.000778961, 0.00121699, 0.00175221, 0.00238456, 0.00311394, 0.00394022, 0.00486329, 0.00588301, 0.00699921, 0.00821174, 0.00952039, 0.010925, 0.0124253, 0.0140211, 0.0157121, 0.0174981, 0.0193789, 0.021354, 0.0234233, 0.0255864, 0.0278429, 0.0301926, 0.0326351, 0.03517, 0.0377968, 0.0405153, 0.0433249, 0.0462254, 0.0492161, 0.0522967, 0.0554667, 0.0587256, 0.0620729, 0.0655081, 0.0690306, 0.07264, 0.0763357, 0.080117, 0.0839836, 0.0879346, 0.0919696, 0.0960879, 0.100289, 0.104572, 0.108936, 0.113381, 0.117907, 0.122511, 0.127194, 0.131956, 0.136794, 0.141709, 0.146699, 0.151765, 0.156904, 0.162117, 0.167403, 0.17276, 0.178189, 0.183687, 0.189255, 0.194891, 0.200595, 0.206365, 0.212202, 0.218103, 0.224068, 0.230096, 0.236187, 0.242339, 0.248551, 0.254822, 0.261152, 0.267539, 0.273983, 0.280483, 0.287036, 0.293643, 0.300303, 0.307014, 0.313776, 0.320587, 0.327446, 0.334353, 0.341306, 0.348304, 0.355346, 0.362431, 0.369558, 0.376726, 0.383934, 0.39118, 0.398464, 0.405784, 0.41314, 0.42053, 0.427953, 0.435407, 0.442893, 0.450408, 0.457952, 0.465523, 0.47312, 0.480742, 0.488389, 0.496057, 0.503748, 0.511458, 0.519188, 0.526936, 0.5347, 0.54248, 0.550275, 0.558082, 0.565902, 0.573733, 0.581573, 0.589421, 0.597277, 0.605139, 0.613005, 0.620876, 0.628748, 0.636622 },
                { 0, 2.0076e-07, 1.60604e-06, 5.42017e-06, 1.28471e-05, 2.50903e-05, 4.33524e-05, 6.88352e-05, 0.000102739, 0.000146264, 0.000200607, 0.000266965, 0.000346533, 0.000440502, 0.000550062, 0.000676403, 0.000820708, 0.00098416, 0.00116794, 0.00137322, 0.00160118, 0.00185298, 0.0021298, 0.00243279, 0.00276312, 0.00312193, 0.00351037, 0.0039296, 0.00438075, 0.00486496, 0.00538335, 0.00593706, 0.0065272, 0.00715488, 0.00782123, 0.00852733, 0.00927428, 0.0100632, 0.0108951, 0.0117711, 0.0126924, 0.0136598, 0.0146746, 0.0157377, 0.0168501, 0.018013, 0.0192274, 0.0204942, 0.0218145, 0.0231893, 0.0246196, 0.0261063, 0.0276505, 0.029253, 0.0309149, 0.0326372, 0.0344207, 0.0362664, 0.0381752, 0.040148, 0.0421858, 0.0442894, 0.0464596, 0.0486975, 0.0510038, 0.0533794, 0.0558251, 0.0583417, 0.0609302, 0.0635912, 0.0663257, 0.0691342, 0.0720178, 0.074977, 0.0780128, 0.0811257, 0.0843165, 0.087586, 0.0909349, 0.0943638, 0.0978734, 0.101464, 0.105137, 0.108893, 0.112732, 0.116655, 0.120663, 0.124755, 0.128934, 0.133199, 0.13755, 0.141989, 0.146516, 0.151131, 0.155835, 0.160628, 0.165512, 0.170485, 0.17555, 0.180705, 0.185952, 0.191292, 0.196723, 0.202247, 0.207865, 0.213576, 0.219381, 0.22528, 0.231273, 0.237361, 0.243544, 0.249822, 0.256196, 0.262665, 0.269231, 0.275892, 0.28265, 0.289504, 0.296455, 0.303503, 0.310648, 0.31789, 0.325228, 0.332664, 0.340198, 0.347828, 0.355556, 0.363382 },
                { 0, 6.18376e-05, 0.0002467, 0.00055361, 0.000981591, 0.00152967, 0.00219686, 0.0029822, 0.00388471, 0.0049034, 0.00603731, 0.00728545, 0.00864685, 0.0101205, 0.0117055, 0.0134009, 0.0152055, 0.0171186, 0.0191391, 0.021266, 0.0234983, 0.0258351, 0.0282754, 0.0308182, 0.0334626, 0.0362076, 0.0390521, 0.0419952, 0.045036, 0.0481735, 0.0514066, 0.0547344, 0.058156, 0.0616703, 0.0652765, 0.0689734, 0.0727601, 0.0766357, 0.0805992, 0.0846495, 0.0887858, 0.093007, 0.0973123, 0.1017, 0.106171, 0.110722, 0.115353, 0.120064, 0.124852, 0.129718, 0.13466, 0.139677, 0.144768, 0.149932, 0.155169, 0.160477, 0.165855, 0.171303, 0.176819, 0.182402, 0.188051, 0.193767, 0.199546, 0.205389, 0.211295, 0.217262, 0.223289, 0.229376, 0.235522, 0.241725, 0.247985, 0.254301, 0.260671, 0.267095, 0.273572, 0.2801, 0.286679, 0.293308, 0.299986, 0.306712, 0.313484, 0.320303, 0.327166, 0.334073, 0.341023, 0.348016, 0.355049, 0.362122, 0.369234, 0.376385, 0.383572, 0.390796, 0.398055, 0.405348, 0.412674, 0.420032, 0.427422, 0.434842, 0.442291, 0.449768, 0.457273, 0.464804, 0.472361, 0.479942, 0.487546, 0.495173, 0.502821, 0.51049, 0.518179, 0.525885, 0.53361, 0.541351, 0.549108, 0.556879, 0.564664, 0.572462, 0.580271, 0.588091, 0.595921, 0.60376, 0.611607, 0.61946, 0.627319, 0.635183, 0.643051, 0.650922, 0.658795, 0.666669 },
                { 0, 1.62731e-07, 1.30185e-06, 4.39373e-06, 1.04148e-05, 2.03413e-05, 3.51498e-05, 5.58166e-05, 8.33181e-05, 0.000118631, 0.000162731, 0.000216594, 0.000281198, 0.000357519, 0.000446533, 0.000549216, 0.000666545, 0.000799495, 0.000949045, 0.00111617, 0.00130184, 0.00150705, 0.00173276, 0.00197994, 0.00224959, 0.00254267, 0.00286015, 0.00320303, 0.00357226, 0.00396884, 0.00439373, 0.00484791, 0.00533236, 0.00584805, 0.00639596, 0.00697707, 0.00759236, 0.00824279, 0.00892935, 0.00965302, 0.0104148, 0.0112156, 0.0120564, 0.0129382, 0.013862, 0.0148288, 0.0158395, 0.0168952, 0.0179967, 0.0191451, 0.0203413, 0.0215864, 0.0228812, 0.0242268, 0.0256242, 0.0270743, 0.0285781, 0.0301366, 0.0317507, 0.0334214, 0.0351498, 0.0369368, 0.0387833, 0.0406903, 0.0426589, 0.0446899, 0.0467844, 0.0489433, 0.0511677, 0.0534585, 0.0558166, 0.0582431, 0.0607389, 0.063305, 0.0659423, 0.068652, 0.0714348, 0.0742919, 0.0772241, 0.0802325, 0.0833181, 0.0864817, 0.0897244, 0.0930472, 0.0964511, 0.0999369, 0.103506, 0.107159, 0.110896, 0.11472, 0.118631, 0.122629, 0.126716, 0.130894, 0.135161, 0.139521, 0.143974, 0.14852, 0.153161, 0.157897, 0.162731, 0.167662, 0.172691, 0.17782, 0.18305, 0.188381, 0.193815, 0.199352, 0.204994, 0.210741, 0.216594, 0.222555, 0.228625, 0.234804, 0.241093, 0.247493, 0.254006, 0.260631, 0.267372, 0.274227, 0.281198, 0.288287, 0.295494, 0.30282, 0.310266, 0.317833, 0.325522, 0.333335 },
                { 0, 4.0151e-07, 3.21178e-06, 1.08381e-05, 2.56848e-05, 5.01518e-05, 8.66332e-05, 0.000137516, 0.000205177, 0.000291985, 0.000400295, 0.00053245, 0.000690779, 0.000877593, 0.00109519, 0.00134584, 0.0016318, 0.00195531, 0.00231857, 0.00272378, 0.00317309, 0.00366864, 0.00421254, 0.00480685, 0.00545363, 0.00615489, 0.00691261, 0.00772874, 0.00860518, 0.0095438, 0.0105464, 0.0116149, 0.0127509, 0.0139563, 0.0152325, 0.0165814, 0.0180044, 0.0195032, 0.0210791, 0.0227337, 0.0244683, 0.0262844, 0.0281831, 0.0301657, 0.0322335, 0.0343875, 0.0366289, 0.0389587, 0.0413779, 0.0438874, 0.0464881, 0.0491808, 0.0519662, 0.0548452, 0.0578182, 0.060886, 0.064049, 0.0673077, 0.0706625, 0.0741139, 0.077662, 0.0813071, 0.0850494, 0.0888891, 0.0928261, 0.0968605, 0.100992, 0.105221, 0.109547, 0.11397, 0.118489, 0.123104, 0.127815, 0.132622, 0.137523, 0.142518, 0.147606, 0.152788, 0.158061, 0.163426, 0.16888, 0.174425, 0.180057, 0.185777, 0.191584, 0.197475, 0.20345, 0.209508, 0.215648, 0.221867, 0.228165, 0.234541, 0.240992, 0.247517, 0.254115, 0.260783, 0.267521, 0.274327, 0.281198, 0.288134, 0.295131, 0.302189, 0.309306, 0.316478, 0.323706, 0.330985, 0.338316, 0.345694, 0.353119, 0.360588, 0.368098, 0.375649, 0.383237, 0.39086, 0.398517, 0.406204, 0.41392, 0.421662, 0.429427, 0.437215, 0.445021, 0.452844, 0.460682, 0.468531, 0.476391, 0.484257, 0.492128, 0.500002 },
                { 0, 3.25462e-07, 2.60369e-06, 8.78745e-06, 2.08295e-05, 4.06827e-05, 7.02996e-05, 0.000111633, 0.000166636, 0.000237261, 0.000325461, 0.000433189, 0.000562397, 0.000715038, 0.000893066, 0.00109843, 0.00133309, 0.00159899, 0.00189809, 0.00223234, 0.00260369, 0.0030141, 0.00346551, 0.00395989, 0.00449918, 0.00508533, 0.00572031, 0.00640605, 0.00714452, 0.00793767, 0.00878745, 0.00969581, 0.0106647, 0.0116961, 0.0127919, 0.0139541, 0.0151847, 0.0164856, 0.0178587, 0.019306, 0.0208295, 0.0224311, 0.0241128, 0.0258764, 0.0277241, 0.0296577, 0.0316791, 0.0337904, 0.0359934, 0.0382902, 0.0406827, 0.0431728, 0.0457624, 0.0484537, 0.0512484, 0.0541486, 0.0571562, 0.0602731, 0.0635014, 0.0668429, 0.0702996, 0.0738735, 0.0775665, 0.0813806, 0.0853176, 0.0893776, 0.0935586, 0.0978588, 0.102276, 0.106809, 0.111454, 0.116212, 0.121078, 0.126052, 0.131131, 0.136314, 0.141598, 0.146982, 0.152464, 0.158041, 0.163712, 0.169475, 0.175327, 0.181268, 0.187294, 0.193405, 0.199597, 0.20587, 0.21222, 0.218647, 0.225148, 0.231721, 0.238364, 0.245076, 0.251854, 0.258697, 0.265602, 0.272568, 0.279592, 0.286673, 0.293809, 0.300997, 0.308236, 0.315524, 0.322859, 0.330238, 0.337661, 0.345125, 0.352627, 0.360167, 0.367742, 0.37535, 0.38299, 0.390658, 0.398354, 0.406076, 0.413821, 0.421587, 0.429373, 0.437176, 0.444995, 0.452828, 0.460672, 0.468527, 0.476389, 0.484256, 0.492128, 0.500002 },
                { 0, 9.25133e-05, 0.000368112, 0.000823902, 0.00145701, 0.00226459, 0.00324382, 0.00439188, 0.00570602, 0.00718346, 0.00882147, 0.0106174, 0.0125684, 0.014672, 0.0169254, 0.0193261, 0.0218715, 0.0245589, 0.0273859, 0.0303499, 0.0334484, 0.036679, 0.0400391, 0.0435264, 0.0471384, 0.0508727, 0.0547271, 0.058699, 0.0627863, 0.0669866, 0.0712977, 0.0757173, 0.0802432, 0.0848731, 0.089605, 0.0944366, 0.0993657, 0.10439, 0.109509, 0.114718, 0.120017, 0.125403, 0.130874, 0.136429, 0.142065, 0.14778, 0.153573, 0.159442, 0.165385, 0.171399, 0.177484, 0.183637, 0.189856, 0.196141, 0.202488, 0.208898, 0.215366, 0.221893, 0.228477, 0.235115, 0.241807, 0.248551, 0.255344, 0.262187, 0.269077, 0.276012, 0.282992, 0.290015, 0.297079, 0.304183, 0.311327, 0.318507, 0.325724, 0.332976, 0.340261, 0.347579, 0.354928, 0.362307, 0.369715, 0.37715, 0.384612, 0.3921, 0.399612, 0.407147, 0.414704, 0.422283, 0.429883, 0.437501, 0.445138, 0.452793, 0.460464, 0.468151, 0.475853, 0.48357, 0.491299, 0.499041, 0.506795, 0.51456, 0.522335, 0.53012, 0.537914, 0.545717, 0.553527, 0.561345, 0.569169, 0.576999, 0.584835, 0.592676, 0.600521, 0.608371, 0.616224, 0.624081, 0.631941, 0.639803, 0.647668, 0.655534, 0.663402, 0.671272, 0.679143, 0.687014, 0.694887, 0.70276, 0.710633, 0.718507, 0.726381, 0.734255, 0.742129, 0.750003 },
                { 0, 9.61011e-10, 1.53761e-08, 7.78416e-08, 2.46018e-07, 6.0063e-07, 1.24547e-06, 2.30738e-06, 3.93629e-06, 6.30517e-06, 9.61008e-06, 1.40701e-05, 1.99275e-05, 2.74473e-05, 3.69181e-05, 4.8651e-05, 6.29806e-05, 8.02643e-05, 0.000100883, 0.000125239, 0.000153761, 0.000186898, 0.000225122, 0.000268929, 0.000318839, 0.000375394, 0.000439157, 0.000510719, 0.000590689, 0.000679702, 0.000778416, 0.000887511, 0.00100769, 0.00113968, 0.00128423, 0.00144211, 0.00161412, 0.00180108, 0.00200383, 0.00222323, 0.00246018, 0.00271558, 0.00299036, 0.00328549, 0.00360195, 0.00394073, 0.00430287, 0.00468941, 0.00510143, 0.00554002, 0.0060063, 0.00650141, 0.00702652, 0.00758281, 0.0081715, 0.00879382, 0.00945102, 0.0101444, 0.0108752, 0.0116449, 0.0124547, 0.013306, 0.0142002, 0.0151387, 0.016123, 0.0171546, 0.0182349, 0.0193654, 0.0205477, 0.0217833, 0.0230738, 0.0244208, 0.025826, 0.0272909, 0.0288173, 0.0304069, 0.0320613, 0.0337823, 0.0355717, 0.0374313, 0.0393629, 0.0413682, 0.0434492, 0.0456078, 0.0478458, 0.0501652, 0.0525679, 0.0550559, 0.0576312, 0.0602958, 0.0630517, 0.0659011, 0.0688459, 0.0718884, 0.0750306, 0.0782747, 0.0816228, 0.0850773, 0.0886403, 0.092314, 0.0961008, 0.100003, 0.104023, 0.108162, 0.112424, 0.116811, 0.121325, 0.125968, 0.130744, 0.135654, 0.140701, 0.145888, 0.151216, 0.15669, 0.16231, 0.168081, 0.174004, 0.180082, 0.186318, 0.192715, 0.199275, 0.206, 0.212895, 0.219962, 0.227203, 0.234621, 0.242219, 0.250001 },
                { 0, 0.000123028, 0.000488253, 0.00108995, 0.0019225, 0.00298035, 0.00425806, 0.00575026, 0.00745168, 0.00935714, 0.0114615, 0.0137598, 0.0162471, 0.0189185, 0.0217694, 0.0247949, 0.0279905, 0.0313517, 0.0348741, 0.0385532, 0.0423849, 0.0463649, 0.0504891, 0.0547535, 0.059154, 0.0636869, 0.0683483, 0.0731344, 0.0780416, 0.0830662, 0.0882049, 0.093454, 0.0988102, 0.10427, 0.109831, 0.115489, 0.121241, 0.127085, 0.133017, 0.139034, 0.145133, 0.151313, 0.157569, 0.1639, 0.170303, 0.176774, 0.183313, 0.189916, 0.196581, 0.203305, 0.210087, 0.216925, 0.223815, 0.230757, 0.237747, 0.244785, 0.251868, 0.258994, 0.266162, 0.273369, 0.280615, 0.287897, 0.295214, 0.302564, 0.309946, 0.317358, 0.324799, 0.332268, 0.339762, 0.347282, 0.354825, 0.36239, 0.369977, 0.377584, 0.38521, 0.392854, 0.400515, 0.408193, 0.415885, 0.423591, 0.431311, 0.439044, 0.446788, 0.454543, 0.462309, 0.470084, 0.477869, 0.485661, 0.493461, 0.501269, 0.509083, 0.516903, 0.524729, 0.532561, 0.540396, 0.548237, 0.556081, 0.563929, 0.57178, 0.579634, 0.587491, 0.59535, 0.603211, 0.611074, 0.618939, 0.626805, 0.634673, 0.642542, 0.650411, 0.658282, 0.666153, 0.674025, 0.681897, 0.68977, 0.697643, 0.705516, 0.71339, 0.721263, 0.729137, 0.737011, 0.744885, 0.752759, 0.760633, 0.768507, 0.776381, 0.784255, 0.792129, 0.800003 },
                { 0, 6.05363e-12, 1.93715e-10, 1.47102e-09, 6.19888e-09, 1.89175e-08, 4.70727e-08, 1.01743e-07, 1.98364e-07, 3.57459e-07, 6.05359e-07, 9.74937e-07, 1.50633e-06, 2.24766e-06, 3.25577e-06, 4.59695e-06, 6.34765e-06, 8.59523e-06, 1.14387e-05, 1.49893e-05, 1.93715e-05, 2.47235e-05, 3.1198e-05, 3.8963e-05, 4.82025e-05, 5.91171e-05, 7.1925e-05, 8.68624e-05, 0.000104185, 0.000124166, 0.000147102, 0.000173309, 0.000203125, 0.00023691, 0.000275047, 0.000317946, 0.000366037, 0.00041978, 0.000479657, 0.00054618, 0.000619888, 0.000701346, 0.000791151, 0.000889929, 0.000998335, 0.00111706, 0.00124682, 0.00138836, 0.00154248, 0.00170999, 0.00189175, 0.00208864, 0.0023016, 0.00253158, 0.0027796, 0.00304668, 0.0033339, 0.0036424, 0.00397332, 0.00432786, 0.00470727, 0.00511284, 0.00554589, 0.0060078, 0.00649999, 0.00702392, 0.00758111, 0.0081731, 0.00880152, 0.00946801, 0.0101743, 0.0109221, 0.0117132, 0.0125495, 0.013433, 0.0143655, 0.015349, 0.0163858, 0.0174778, 0.0186272, 0.0198364, 0.0211076, 0.0224431, 0.0238453, 0.0253168, 0.0268601, 0.0284777, 0.0301724, 0.0319467, 0.0338036, 0.0357458, 0.0377764, 0.0398981, 0.0421141, 0.0444275, 0.0468415, 0.0493593, 0.0519842, 0.0547197, 0.057569, 0.0605359, 0.0636238, 0.0668365, 0.0701777, 0.0736512, 0.0772609, 0.0810107, 0.0849047, 0.0889471, 0.093142, 0.0974937, 0.102007, 0.106685, 0.111533, 0.116557, 0.121759, 0.127146, 0.132722, 0.138491, 0.14446, 0.150633, 0.157015, 0.163611, 0.170427, 0.177469, 0.184741, 0.19225, 0.200001 },
                { 0, 3.84404e-09, 6.15045e-08, 3.11367e-07, 9.84072e-07, 2.40252e-06, 4.98186e-06, 9.22952e-06, 1.57451e-05, 2.52207e-05, 3.84403e-05, 5.62804e-05, 7.97098e-05, 0.000109789, 0.000147672, 0.000194604, 0.000251922, 0.000321057, 0.000403531, 0.000500958, 0.000615045, 0.000747591, 0.000900487, 0.00107572, 0.00127536, 0.00150157, 0.00175663, 0.00204288, 0.00236276, 0.00271881, 0.00311366, 0.00355004, 0.00403076, 0.00455872, 0.00513692, 0.00576845, 0.00645649, 0.00720433, 0.00801533, 0.00889294, 0.00984072, 0.0108623, 0.0119615, 0.013142, 0.0144078, 0.0157629, 0.0172115, 0.0187576, 0.0204057, 0.0221601, 0.0240252, 0.0260056, 0.0281061, 0.0303312, 0.032686, 0.0351753, 0.0378041, 0.0405776, 0.043501, 0.0465795, 0.0498186, 0.0532238, 0.0568007, 0.0605549, 0.0644919, 0.0686117, 0.0729089, 0.0773778, 0.0820127, 0.0868082, 0.0917589, 0.0968594, 0.102105, 0.107489, 0.113009, 0.118658, 0.124431, 0.130325, 0.136334, 0.142453, 0.148679, 0.155007, 0.161433, 0.167952, 0.17456, 0.181253, 0.188028, 0.194881, 0.201807, 0.208803, 0.215867, 0.222993, 0.230179, 0.237421, 0.244717, 0.252063, 0.259457, 0.266894, 0.274373, 0.281891, 0.289446, 0.297033, 0.304652, 0.3123, 0.319975, 0.327673, 0.335395, 0.343136, 0.350896, 0.358673, 0.366464, 0.374269, 0.382086, 0.389913, 0.397749, 0.405593, 0.413444, 0.4213, 0.429161, 0.437025, 0.444893, 0.452763, 0.460634, 0.468507, 0.47638, 0.484254, 0.492128, 0.500002 },
                { 0, 4.8429e-11, 1.54972e-09, 1.17682e-08, 4.9591e-08, 1.5134e-07, 3.76582e-07, 8.13942e-07, 1.58691e-06, 2.85967e-06, 4.84287e-06, 7.79949e-06, 1.20506e-05, 1.79812e-05, 2.60461e-05, 3.67756e-05, 5.07812e-05, 6.87619e-05, 9.15094e-05, 0.000119914, 0.000154972, 0.000197788, 0.000249584, 0.000311704, 0.00038562, 0.000472937, 0.0005754, 0.000694899, 0.000833476, 0.000993329, 0.00117682, 0.00138647, 0.001625, 0.00189528, 0.00220038, 0.00254357, 0.0029283, 0.00335824, 0.00383726, 0.00436944, 0.0049591, 0.00561077, 0.00632921, 0.00711943, 0.00798668, 0.00893646, 0.00997452, 0.0111069, 0.0123398, 0.0136799, 0.015134, 0.0167091, 0.0184128, 0.0202527, 0.0222368, 0.0243734, 0.0266712, 0.0291392, 0.0317865, 0.0346229, 0.0376582, 0.0409027, 0.0443671, 0.0480624, 0.0519995, 0.0561782, 0.0605878, 0.0652173, 0.0700561, 0.0750938, 0.0803205, 0.0857266, 0.0913028, 0.0970402, 0.10293, 0.108964, 0.115135, 0.121434, 0.127854, 0.134388, 0.141029, 0.14777, 0.154606, 0.161531, 0.168537, 0.175621, 0.182777, 0.189999, 0.197284, 0.204625, 0.21202, 0.219465, 0.226954, 0.234485, 0.242054, 0.249657, 0.257293, 0.264957, 0.272648, 0.280362, 0.288098, 0.295852, 0.303624, 0.31141, 0.319211, 0.327022, 0.334845, 0.342676, 0.350515, 0.358361, 0.366212, 0.374068, 0.381928, 0.389791, 0.397657, 0.405525, 0.413395, 0.421266, 0.429138, 0.437011, 0.444884, 0.452758, 0.460632, 0.468506, 0.47638, 0.484254, 0.492128, 0.500002 },
                { 0, 0.000153383, 0.000607138, 0.00135184, 0.0023783, 0.00367752, 0.00524076, 0.00705945, 0.00912526, 0.0114301, 0.0139659, 0.0167251, 0.0197, 0.0228834, 0.026268, 0.029847, 0.0336135, 0.0375609, 0.0416828, 0.045973, 0.0504252, 0.0550337, 0.0597926, 0.0646964, 0.0697396, 0.074917, 0.0802233, 0.0856537, 0.0912032, 0.0968672, 0.102641, 0.108521, 0.114501, 0.120579, 0.12675, 0.13301, 0.139355, 0.145782, 0.152287, 0.158867, 0.165519, 0.172239, 0.179024, 0.185871, 0.192777, 0.19974, 0.206757, 0.213826, 0.220943, 0.228106, 0.235314, 0.242563, 0.249853, 0.25718, 0.264542, 0.271939, 0.279368, 0.286827, 0.294314, 0.301829, 0.309369, 0.316933, 0.324519, 0.332127, 0.339755, 0.347402, 0.355066, 0.362747, 0.370443, 0.378154, 0.385878, 0.393615, 0.401364, 0.409123, 0.416892, 0.424671, 0.432459, 0.440255, 0.448058, 0.455868, 0.463685, 0.471507, 0.479334, 0.487167, 0.495004, 0.502845, 0.510689, 0.518537, 0.526388, 0.534242, 0.542099, 0.549957, 0.557818, 0.56568, 0.573544, 0.58141, 0.589276, 0.597144, 0.605013, 0.612882, 0.620753, 0.628624, 0.636495, 0.644367, 0.652239, 0.660112, 0.667985, 0.675858, 0.683732, 0.691605, 0.699479, 0.707352, 0.715226, 0.7231, 0.730974, 0.738848, 0.746722, 0.754596, 0.76247, 0.770344, 0.778218, 0.786092, 0.793966, 0.80184, 0.809714, 0.817588, 0.825462, 0.833337 },
                { 0, 3.9722e-14, 2.5422e-12, 2.89572e-11, 1.627e-10, 6.20652e-10, 1.85326e-09, 4.67322e-09, 1.04128e-08, 2.11098e-08, 3.97217e-08, 7.03695e-08, 1.18608e-07, 1.91729e-07, 2.99086e-07, 4.52455e-07, 6.6642e-07, 9.58786e-07, 1.35102e-06, 1.86874e-06, 2.54219e-06, 3.40678e-06, 4.50365e-06, 5.88024e-06, 7.59094e-06, 9.69769e-06, 1.22707e-05, 1.5389e-05, 1.91415e-05, 2.36274e-05, 2.89571e-05, 3.52532e-05, 4.26509e-05, 5.12993e-05, 6.13623e-05, 7.30191e-05, 8.64655e-05, 0.000101915, 0.0001196, 0.000139771, 0.0001627, 0.000188682, 0.000218034, 0.000251095, 0.000288233, 0.00032984, 0.000376335, 0.000428169, 0.00048582, 0.0005498, 0.000620652, 0.000698955, 0.000785323, 0.000880407, 0.000984897, 0.00109952, 0.00122506, 0.00136231, 0.00151215, 0.00167548, 0.00185326, 0.00204648, 0.0022562, 0.00248354, 0.00272966, 0.00299577, 0.00328316, 0.00359316, 0.00392719, 0.0042867, 0.00467322, 0.00508836, 0.00553379, 0.00601126, 0.00652256, 0.00706961, 0.00765437, 0.0082789, 0.00894532, 0.00965585, 0.0104128, 0.0112186, 0.0120757, 0.0129866, 0.0139542, 0.014981, 0.0160701, 0.0172244, 0.0184469, 0.019741, 0.0211098, 0.0225567, 0.0240855, 0.0256996, 0.0274028, 0.0291991, 0.0310925, 0.0330871, 0.0351872, 0.0373972, 0.0397217, 0.0421654, 0.0447331, 0.0474298, 0.0502607, 0.0532309, 0.056346, 0.0596116, 0.0630334, 0.0666173, 0.0703695, 0.0742961, 0.0784036, 0.0826987, 0.0871881, 0.0918788, 0.0967779, 0.101893, 0.107231, 0.1128, 0.118608, 0.124664, 0.130975, 0.137549, 0.144397, 0.151526, 0.158947, 0.166667 },
                { 0, 0.00021102, 0.000829069, 0.00183253, 0.00320092, 0.00491487, 0.00695601, 0.00930698, 0.0119513, 0.0148734, 0.0180586, 0.0214927, 0.0251628, 0.0290561, 0.0331608, 0.0374658, 0.0419603, 0.0466344, 0.0514784, 0.0564834, 0.0616407, 0.0669424, 0.0723807, 0.0779483, 0.0836385, 0.0894447, 0.0953607, 0.101381, 0.107499, 0.113711, 0.120011, 0.126395, 0.132857, 0.139395, 0.146004, 0.15268, 0.159419, 0.166219, 0.173076, 0.179987, 0.186949, 0.19396, 0.201016, 0.208116, 0.215257, 0.222437, 0.229653, 0.236905, 0.24419, 0.251506, 0.258852, 0.266225, 0.273626, 0.281051, 0.2885, 0.295972, 0.303465, 0.310979, 0.318511, 0.326062, 0.33363, 0.341214, 0.348814, 0.356428, 0.364056, 0.371697, 0.37935, 0.387015, 0.394692, 0.402378, 0.410075, 0.417781, 0.425496, 0.43322, 0.440951, 0.44869, 0.456436, 0.464189, 0.471949, 0.479714, 0.487486, 0.495262, 0.503044, 0.510831, 0.518622, 0.526418, 0.534218, 0.542022, 0.54983, 0.557641, 0.565455, 0.573273, 0.581094, 0.588917, 0.596744, 0.604572, 0.612403, 0.620237, 0.628072, 0.63591, 0.64375, 0.651591, 0.659434, 0.667279, 0.675125, 0.682973, 0.690822, 0.698673, 0.706524, 0.714377, 0.722231, 0.730086, 0.737943, 0.7458, 0.753658, 0.761516, 0.769376, 0.777236, 0.785098, 0.792959, 0.800822, 0.808685, 0.816549, 0.824413, 0.832278, 0.840143, 0.848009, 0.855875 },
                { 0, 7.90318e-06, 1.62497e-05, 2.50645e-05, 3.43737e-05, 4.42051e-05, 5.4588e-05, 6.55534e-05, 7.71338e-05, 8.93639e-05, 0.00010228, 0.000115921, 0.000130326, 0.00014554, 0.000161608, 0.000178576, 0.000196497, 0.000215423, 0.00023541, 0.000256519, 0.000278811, 0.000302355, 0.000327219, 0.000353477, 0.000381209, 0.000410496, 0.000441426, 0.000474092, 0.000508589, 0.000545022, 0.000583498, 0.000624133, 0.000667047, 0.000712369, 0.000760233, 0.000810781, 0.000864166, 0.000920545, 0.000980086, 0.00104297, 0.00110938, 0.00117951, 0.00125358, 0.0013318, 0.00141441, 0.00150166, 0.0015938, 0.00169111, 0.00179387, 0.0019024, 0.00201702, 0.00213807, 0.00226591, 0.00240092, 0.00254351, 0.00269409, 0.00285312, 0.00302107, 0.00319844, 0.00338576, 0.00358359, 0.00379251, 0.00401316, 0.00424618, 0.00449228, 0.00475218, 0.00502665, 0.00531653, 0.00562267, 0.00594597, 0.00628742, 0.00664802, 0.00702884, 0.00743103, 0.00785578, 0.00830436, 0.0087781, 0.00927841, 0.00980679, 0.0103648, 0.0109541, 0.0115765, 0.0122338, 0.012928, 0.0136611, 0.0144353, 0.015253, 0.0161165, 0.0170284, 0.0179916, 0.0190087, 0.0200829, 0.0212174, 0.0224155, 0.0236808, 0.0250171, 0.0264283, 0.0279187, 0.0294927, 0.0311551, 0.0329106, 0.0347646, 0.0367227, 0.0387906, 0.0409744, 0.0432808, 0.0457166, 0.048289, 0.0510057, 0.0538747, 0.0569048, 0.0601048, 0.0634843, 0.0670534, 0.0708226, 0.0748034, 0.0790074, 0.0834473, 0.0881362, 0.0930881, 0.0983178, 0.103841, 0.109674, 0.115834, 0.12234, 0.12921, 0.136466, 0.144129 },
                { 0, 0.00065797, 0.00185881, 0.0034108, 0.00524502, 0.00732139, 0.00961269, 0.0120988, 0.0147641, 0.0175959, 0.0205837, 0.0237183, 0.0269922, 0.0303985, 0.033931, 0.0375845, 0.041354, 0.0452351, 0.0492236, 0.0533158, 0.0575083, 0.0617977, 0.0661811, 0.0706557, 0.0752187, 0.0798677, 0.0846003, 0.0894143, 0.0943074, 0.0992777, 0.104323, 0.109442, 0.114633, 0.119893, 0.125222, 0.130617, 0.136077, 0.141602, 0.147188, 0.152835, 0.158542, 0.164307, 0.17013, 0.176008, 0.181941, 0.187927, 0.193966, 0.200056, 0.206196, 0.212386, 0.218624, 0.224909, 0.231241, 0.237618, 0.244039, 0.250504, 0.257011, 0.263561, 0.270151, 0.276782, 0.283451, 0.29016, 0.296906, 0.303689, 0.310508, 0.317362, 0.324251, 0.331174, 0.338131, 0.34512, 0.352141, 0.359192, 0.366275, 0.373387, 0.380528, 0.387698, 0.394895, 0.40212, 0.409371, 0.416649, 0.423951, 0.431278, 0.43863, 0.446005, 0.453402, 0.460822, 0.468264, 0.475727, 0.483211, 0.490714, 0.498237, 0.505779, 0.513339, 0.520917, 0.528512, 0.536124, 0.543752, 0.551395, 0.559054, 0.566727, 0.574414, 0.582115, 0.589829, 0.597555, 0.605293, 0.613042, 0.620803, 0.628574, 0.636354, 0.644144, 0.651943, 0.65975, 0.667566, 0.675388, 0.683218, 0.691053, 0.698895, 0.706742, 0.714594, 0.72245, 0.730311, 0.738174, 0.746041, 0.75391, 0.761781, 0.769654, 0.777527, 0.785401 },
                { 0, 8.13662e-08, 6.50947e-07, 2.19705e-06, 5.20816e-06, 1.0173e-05, 1.75808e-05, 2.7921e-05, 4.16839e-05, 5.93601e-05, 8.14411e-05, 0.000108419, 0.000140788, 0.000179042, 0.000223675, 0.000275185, 0.00033407, 0.000400829, 0.000475963, 0.000559973, 0.000653366, 0.000756645, 0.00087032, 0.0009949, 0.0011309, 0.00127883, 0.0014392, 0.00161255, 0.00179939, 0.00200024, 0.00221563, 0.00244609, 0.00269216, 0.00295438, 0.00323327, 0.0035294, 0.0038433, 0.00417553, 0.00452664, 0.00489719, 0.00528775, 0.00569889, 0.00613117, 0.00658518, 0.0070615, 0.00756072, 0.00808343, 0.00863024, 0.00920174, 0.00979856, 0.0104213, 0.0110706, 0.0117471, 0.0124514, 0.0131842, 0.0139461, 0.0147378, 0.01556, 0.0164133, 0.0172984, 0.0182161, 0.019167, 0.0201518, 0.0211713, 0.0222263, 0.0233174, 0.0244455, 0.0256113, 0.0268156, 0.0280591, 0.0293429, 0.0306675, 0.032034, 0.0334432, 0.0348959, 0.036393, 0.0379356, 0.0395244, 0.0411605, 0.0428449, 0.0445785, 0.0463624, 0.0481977, 0.0500854, 0.0520266, 0.0540225, 0.0560742, 0.0581831, 0.0603502, 0.0625769, 0.0648645, 0.0672144, 0.069628, 0.0721067, 0.0746521, 0.0772658, 0.0799494, 0.0827045, 0.085533, 0.0884368, 0.0914177, 0.0944778, 0.0976192, 0.100844, 0.104155, 0.107555, 0.111045, 0.11463, 0.118312, 0.122093, 0.125979, 0.129972, 0.134076, 0.138297, 0.142639, 0.147106, 0.151707, 0.156446, 0.161332, 0.166374, 0.171583, 0.176971, 0.182554, 0.188351, 0.194391, 0.200713, 0.207387, 0.214603 },
                { 0, 0.00014471, 0.000574705, 0.00128383, 0.00226597, 0.00351512, 0.00502529, 0.00679057, 0.00880512, 0.0110632, 0.013559, 0.0162869, 0.0192413, 0.0224166, 0.0258075, 0.0294084, 0.0332141, 0.0372192, 0.0414186, 0.045807, 0.0503794, 0.0551307, 0.060056, 0.0651503, 0.0704088, 0.0758267, 0.0813993, 0.0871219, 0.0929899, 0.0989988, 0.105144, 0.111421, 0.117826, 0.124354, 0.131002, 0.137764, 0.144637, 0.151617, 0.1587, 0.165882, 0.173159, 0.180527, 0.187982, 0.195522, 0.203142, 0.210838, 0.218608, 0.226448, 0.234354, 0.242323, 0.250352, 0.258438, 0.266578, 0.274768, 0.283007, 0.291289, 0.299614, 0.307978, 0.316378, 0.324812, 0.333277, 0.34177, 0.35029, 0.358833, 0.367397, 0.375981, 0.384581, 0.393196, 0.401823, 0.410461, 0.419107, 0.42776, 0.436417, 0.445078, 0.453739, 0.4624, 0.471059, 0.479714, 0.488363, 0.497007, 0.505642, 0.514268, 0.522883, 0.531487, 0.540078, 0.548656, 0.557218, 0.565765, 0.574295, 0.582807, 0.591302, 0.599778, 0.608234, 0.61667, 0.625086, 0.633481, 0.641855, 0.650207, 0.658537, 0.666846, 0.675132, 0.683396, 0.691638, 0.699858, 0.708056, 0.716232, 0.724387, 0.732521, 0.740634, 0.748727, 0.756799, 0.764853, 0.772888, 0.780905, 0.788905, 0.796889, 0.804857, 0.81281, 0.82075, 0.828678, 0.836595, 0.844501, 0.852399, 0.860288, 0.868172, 0.876051, 0.883927, 0.891802 },
                { 0, -2.74303e-07, -2.17365e-06, -7.26598e-06, -1.70569e-05, -3.29897e-05, -5.64455e-05, -8.87428e-05, -0.000131138, -0.000184826, -0.000250937, -0.000330541, -0.000424646, -0.000534196, -0.000660074, -0.0008031, -0.000964032, -0.00114357, -0.00134233, -0.00156091, -0.00179979, -0.00205944, -0.00234024, -0.0026425, -0.00296648, -0.00331239, -0.00368036, -0.00407046, -0.0044827, -0.00491702, -0.00537332, -0.00585142, -0.00635108, -0.00687199, -0.0074138, -0.00797607, -0.00855832, -0.00916, -0.0097805, -0.0104191, -0.0110752, -0.0117478, -0.0124362, -0.0131394, -0.0138564, -0.0145862, -0.0153277, -0.0160797, -0.0168409, -0.0176101, -0.0183859, -0.0191669, -0.0199515, -0.0207383, -0.0215257, -0.0223119, -0.0230952, -0.0238739, -0.024646, -0.0254097, -0.026163, -0.0269037, -0.0276299, -0.0283394, -0.0290298, -0.0296989, -0.0303445, -0.0309639, -0.0315548, -0.0321146, -0.0326407, -0.0331305, -0.0335811, -0.0339899, -0.0343539, -0.0346702, -0.0349359, -0.0351479, -0.0353031, -0.0353984, -0.0354304, -0.035396, -0.0352917, -0.0351142, -0.0348599, -0.0345254, -0.034107, -0.033601, -0.0330038, -0.0323115, -0.0315203, -0.0306262, -0.0296253, -0.0285136, -0.0272869, -0.0259411, -0.024472, -0.0228752, -0.0211465, -0.0192813, -0.0172753, -0.0151238, -0.0128224, -0.0103662, -0.00775067, -0.00497094, -0.00202216, 0.00110057, 0.00440223, 0.00788786, 0.0115625, 0.0154315, 0.0194998, 0.0237729, 0.0282561, 0.0329548, 0.0378744, 0.0430206, 0.0483988, 0.0540148, 0.0598743, 0.0659831, 0.072347, 0.0789719, 0.0858638, 0.0930287, 0.100473, 0.108202 },
                { 0, 1.62737e-07, 1.30204e-06, 4.3952e-06, 1.0421e-05, 2.03603e-05, 3.5197e-05, 5.59188e-05, 8.35176e-05, 0.000118991, 0.000163341, 0.00021758, 0.000282724, 0.000359801, 0.000449846, 0.000553907, 0.000673041, 0.000808319, 0.000960825, 0.00113166, 0.00132194, 0.00153279, 0.00176537, 0.00202086, 0.00230044, 0.00260533, 0.00293677, 0.00329605, 0.00368445, 0.00410332, 0.00455402, 0.00503795, 0.00555657, 0.00611137, 0.00670389, 0.00733572, 0.0080085, 0.00872397, 0.00948389, 0.0102901, 0.0111446, 0.0120494, 0.0130066, 0.0140186, 0.0150875, 0.0162161, 0.017407, 0.018663, 0.0199873, 0.0213833, 0.0228544, 0.0244048, 0.0260388, 0.0277614, 0.0295779, 0.0314947, 0.0335191, 0.0356596, 0.0379266, 0.040333, 0.0428958, 0.0456385, 0.0485978, 0.0518466, 0.0557837, 0.0604089, 0.0653236, 0.070455, 0.0757663, 0.0812339, 0.0868409, 0.0925745, 0.0984241, 0.104381, 0.110439, 0.11659, 0.12283, 0.129154, 0.135557, 0.142035, 0.148585, 0.155203, 0.161886, 0.168631, 0.175437, 0.182299, 0.189215, 0.196185, 0.203204, 0.210272, 0.217386, 0.224545, 0.231746, 0.238988, 0.24627, 0.253589, 0.260944, 0.268335, 0.275758, 0.283213, 0.290699, 0.298213, 0.305756, 0.313325, 0.32092, 0.328538, 0.33618, 0.343843, 0.351527, 0.35923, 0.366951, 0.37469, 0.382445, 0.390215, 0.397999, 0.405796, 0.413605, 0.421425, 0.429254, 0.437093, 0.44494, 0.452793, 0.460652, 0.468516, 0.476384, 0.484255, 0.492128, 0.500002 },
                { 0, -8.30725e-07, -6.53524e-06, -2.16833e-05, -5.05131e-05, -9.6931e-05, -0.000164512, -0.000256499, -0.000375803, -0.000525005, -0.000706353, -0.000921763, -0.00117282, -0.00146078, -0.00178656, -0.00215075, -0.00255361, -0.00299507, -0.00347471, -0.00399182, -0.00454531, -0.00513379, -0.00575552, -0.00640844, -0.00709016, -0.00779795, -0.00852875, -0.00927918, -0.0100455, -0.0108237, -0.0116093, -0.0123977, -0.0131838, -0.0139621, -0.0147271, -0.0154727, -0.0161925, -0.0168797, -0.0175274, -0.0181281, -0.0186742, -0.0191577, -0.0195701, -0.0199028, -0.0201468, -0.0202926, -0.0203307, -0.0202509, -0.0200429, -0.0196961, -0.0191994, -0.0185415, -0.0177106, -0.0166949, -0.0154818, -0.0140588, -0.0124128, -0.0105306, -0.00839837, -0.00600218, -0.00332769, -0.000360228, 0.0029152, 0.00651393, 0.010451, 0.0147263, 0.0193249, 0.0242315, 0.029431, 0.0349089, 0.0406507, 0.0466425, 0.0528706, 0.0593216, 0.0659826, 0.0728409, 0.0798841, 0.0871002, 0.0944775, 0.102005, 0.109671, 0.117465, 0.125377, 0.133397, 0.141515, 0.149722, 0.158008, 0.166366, 0.174786, 0.183261, 0.191783, 0.200344, 0.208938, 0.217557, 0.226196, 0.234849, 0.243509, 0.252171, 0.260831, 0.269483, 0.278123, 0.286748, 0.295353, 0.303935, 0.31249, 0.321017, 0.329513, 0.337976, 0.346403, 0.354794, 0.363148, 0.371464, 0.37974, 0.387979, 0.396179, 0.404341, 0.412466, 0.420555, 0.42861, 0.436634, 0.444627, 0.452593, 0.460535, 0.468455, 0.476358, 0.484247, 0.492127, 0.500002 },
                { 0, 0.000245235, 0.0010908, 0.00267502, 0.00510087, 0.00843666, 0.0127177, 0.0179487, 0.0241068, 0.0311451, 0.0389963, 0.0475771, 0.0567918, 0.0665367, 0.0767036, 0.0871834, 0.0978696, 0.108661, 0.119462, 0.13019, 0.140771, 0.151141, 0.161253, 0.171069, 0.180564, 0.189725, 0.19855, 0.207047, 0.215232, 0.223128, 0.230764, 0.238174, 0.245395, 0.252465, 0.259423, 0.266305, 0.273149, 0.279988, 0.286851, 0.293765, 0.300752, 0.30783, 0.315012, 0.322307, 0.32972, 0.337252, 0.3449, 0.352659, 0.36052, 0.368474, 0.376508, 0.38461, 0.392766, 0.400963, 0.409186, 0.417424, 0.425665, 0.433897, 0.442111, 0.4503, 0.458458, 0.466578, 0.474659, 0.482698, 0.490695, 0.498651, 0.506568, 0.514448, 0.522296, 0.530114, 0.537909, 0.545684, 0.553445, 0.561196, 0.568941, 0.576686, 0.584433, 0.592186, 0.599948, 0.607722, 0.615508, 0.623308, 0.631123, 0.638953, 0.646797, 0.654655, 0.662527, 0.67041, 0.678303, 0.686204, 0.694113, 0.702027, 0.709944, 0.717864, 0.725784, 0.733703, 0.74162, 0.749533, 0.757443, 0.765349, 0.773249, 0.781144, 0.789034, 0.796919, 0.804799, 0.812674, 0.820545, 0.828413, 0.836277, 0.844139, 0.851999, 0.859858, 0.867716, 0.875574, 0.883432, 0.891291, 0.89915, 0.907012, 0.914875, 0.922739, 0.930606, 0.938474, 0.946344, 0.954216, 0.96209, 0.969965, 0.977841, 0.985718 },
                { 0, -3.36526e-06, -5.63301e-06, -6.60454e-06, -6.09943e-06, -3.96345e-06, -7.64145e-08, 5.64e-06, 1.3215e-05, 2.2622e-05, 3.37732e-05, 4.65151e-05, 6.06259e-05, 7.58147e-05, 9.17228e-05, 0.000107927, 0.000123947, 0.000139251, 0.00015327, 0.00016541, 0.000175068, 0.000181651, 0.000184595, 0.000183386, 0.000177584, 0.000166844, 0.000150937, 0.000129775, 0.000103422, 7.21198e-05, 3.6292e-05, -3.44365e-06, -4.62741e-05, -9.11943e-05, -0.000137016, -0.000182387, -0.000225812, -0.000265685, -0.000300328, -0.000328037, -0.000347131, -0.000356008, -0.000353208, -0.000337473, -0.000307812, -0.000263561, -0.000204446, -0.000130632, -4.27718e-05, 5.79613e-05, 0.000169848, 0.000290616, 0.00041745, 0.000547014, 0.000675504, 0.000798708, 0.0009121, 0.00101094, 0.00109041, 0.00114574, 0.0011724, 0.00116622, 0.00112362, 0.00104174, 0.000918675, 0.000753575, 0.000546845, 0.000300258, 1.70591e-05, -0.000297967, -0.000638472, -0.000996565, -0.00136289, -0.00172674, -0.00207626, -0.00239868, -0.00268062, -0.00290845, -0.00306869, -0.00314842, -0.00313585, -0.00302071, -0.00279485, -0.00245267, -0.00199165, -0.00141273, -0.000720726, 7.53874e-05, 0.000962303, 0.00192229, 0.00293324, 0.00396888, 0.00499912, 0.00599058, 0.00690725, 0.00771135, 0.00836432, 0.00882796, 0.0090657, 0.00904392, 0.00873338, 0.00811064, 0.00715951, 0.00587236, 0.00425137, 0.00230959, 7.17794e-05, -0.00242504, -0.00513134, -0.0079852, -0.0109129, -0.0138298, -0.0166419, -0.0192477, -0.0215406, -0.0234114, -0.0247521, -0.0254588, -0.025436, -0.0246002, -0.0228842, -0.0202412, -0.0166482, -0.01211, -0.00666178, -0.000371954, 0.00665653, 0.0142853 },
                { 0, 1.23065e-06, 9.84521e-06, 3.32276e-05, 7.87616e-05, 0.000153831, 0.00026582, 0.000422113, 0.000630093, 0.000897144, 0.00123065, 0.001638, 0.00212656, 0.00270374, 0.0033769, 0.00415344, 0.00504074, 0.00604618, 0.00717715, 0.00844103, 0.0098452, 0.0113971, 0.013104, 0.0149733, 0.0170125, 0.0192289, 0.0216299, 0.0242229, 0.0270152, 0.0300143, 0.0332276, 0.0366623, 0.0403259, 0.0442259, 0.0483695, 0.0527641, 0.0574172, 0.0623361, 0.0675282, 0.0730009, 0.0787616, 0.0848176, 0.0911764, 0.0978453, 0.104832, 0.112143, 0.119787, 0.127599, 0.135255, 0.142754, 0.150103, 0.15731, 0.164382, 0.171326, 0.178151, 0.184862, 0.191468, 0.197976, 0.204394, 0.210728, 0.216987, 0.223177, 0.229306, 0.235381, 0.24141, 0.2474, 0.253358, 0.259292, 0.26521, 0.271118, 0.277024, 0.282935, 0.288859, 0.294804, 0.300775, 0.306782, 0.312831, 0.31893, 0.325086, 0.331306, 0.337598, 0.343969, 0.350427, 0.356979, 0.363632, 0.370394, 0.377272, 0.384273, 0.391406, 0.398676, 0.406093, 0.413662, 0.421392, 0.429238, 0.43702, 0.444729, 0.452372, 0.459957, 0.467491, 0.474981, 0.482435, 0.489861, 0.497264, 0.504654, 0.512037, 0.51942, 0.526812, 0.534219, 0.541649, 0.549109, 0.556606, 0.564149, 0.571744, 0.579399, 0.58712, 0.594917, 0.602776, 0.61061, 0.618412, 0.626191, 0.633953, 0.641707, 0.649458, 0.657216, 0.664986, 0.672777, 0.680596, 0.68845 },
                { 0, 2.0082e-05, 7.54053e-05, 0.000158586, 0.00026224, 0.000378984, 0.000501434, 0.000622206, 0.000733915, 0.000829179, 0.000900612, 0.000940832, 0.000955665, 0.00103337, 0.00118558, 0.00140492, 0.00168401, 0.00201545, 0.00239186, 0.00280587, 0.00325008, 0.00371712, 0.0041996, 0.00469013, 0.00518133, 0.00566582, 0.00613621, 0.00658513, 0.00700517, 0.00738897, 0.00772914, 0.00801829, 0.00824905, 0.00841402, 0.00850582, 0.00853397, 0.00867816, 0.00898278, 0.00944045, 0.0100438, 0.0107854, 0.0116579, 0.0126539, 0.0137661, 0.014987, 0.0163092, 0.0177255, 0.0192283, 0.0208103, 0.0224642, 0.0241825, 0.0259579, 0.0277829, 0.0296502, 0.0315525, 0.0334822, 0.0354321, 0.0373947, 0.0393627, 0.0413287, 0.0432853, 0.0452252, 0.0471408, 0.0490249, 0.0508701, 0.052669, 0.0544141, 0.0560982, 0.0577138, 0.0592535, 0.06071, 0.0620759, 0.0633438, 0.0645063, 0.065556, 0.0664856, 0.0672876, 0.0679547, 0.0684795, 0.0688546, 0.0690727, 0.0691347, 0.0693652, 0.0699279, 0.0708156, 0.0720207, 0.073536, 0.075354, 0.0774674, 0.0798687, 0.0825506, 0.0855058, 0.0887267, 0.0922061, 0.0959366, 0.0999107, 0.104121, 0.10856, 0.113221, 0.118096, 0.123178, 0.128459, 0.133932, 0.13959, 0.145424, 0.151429, 0.157596, 0.163919, 0.170388, 0.176999, 0.183742, 0.19061, 0.197597, 0.204694, 0.211895, 0.219192, 0.226578, 0.234044, 0.241585, 0.249192, 0.256858, 0.264576, 0.272338, 0.280137, 0.287965, 0.295816, 0.303681, 0.311554 },
                { 0, 1.88513e-05, 6.55601e-05, 0.000125358, 0.000183479, 0.000225153, 0.000238916, 0.000296396, 0.000421002, 0.000597966, 0.000812521, 0.0010499, 0.00129533, 0.00153405, 0.00175129, 0.00193229, 0.00206226, 0.00212645, 0.00216954, 0.00236011, 0.00269635, 0.00316348, 0.00374675, 0.00443137, 0.00520258, 0.00604563, 0.00694572, 0.00788811, 0.00885802, 0.00984069, 0.0108213, 0.0117852, 0.0127175, 0.0136035, 0.0144284, 0.0151775, 0.015836, 0.016389, 0.0168219, 0.0171199, 0.0172682, 0.0173413, 0.0177039, 0.018384, 0.0193668, 0.0206377, 0.0221817, 0.0239841, 0.0260303, 0.0283053, 0.0307945, 0.033483, 0.0363561, 0.0393991, 0.0425971, 0.0459354, 0.0493992, 0.0529738, 0.0566444, 0.0603962, 0.0642145, 0.0680845, 0.0719913, 0.0759204, 0.0798574, 0.0838024, 0.0877696, 0.0917737, 0.0958294, 0.0999517, 0.104155, 0.108455, 0.112865, 0.117401, 0.122077, 0.126908, 0.131909, 0.137094, 0.142479, 0.148078, 0.153906, 0.159977, 0.166308, 0.172911, 0.179802, 0.186996, 0.194507, 0.202308, 0.210034, 0.21761, 0.225051, 0.232372, 0.239588, 0.246713, 0.253762, 0.26075, 0.267692, 0.274602, 0.281495, 0.288387, 0.295291, 0.302222, 0.309196, 0.316227, 0.32333, 0.33052, 0.33781, 0.345217, 0.352755, 0.360439, 0.36827, 0.376079, 0.383824, 0.391517, 0.399173, 0.406809, 0.414437, 0.422074, 0.429733, 0.437431, 0.44518, 0.452997, 0.460857, 0.468689, 0.476505, 0.484319, 0.492147, 0.500002 },
                { 0, 1.14563e-06, 3.33872e-06, 6.68788e-06, 1.12507e-05, 1.70176e-05, 2.38959e-05, 3.16957e-05, 4.01192e-05, 4.87542e-05, 5.70736e-05, 6.44429e-05, 7.01365e-05, 7.3364e-05, 7.33074e-05, 6.91694e-05, 6.02313e-05, 4.59211e-05, 2.58871e-05, 7.5197e-08, -3.11945e-05, -6.71583e-05, -0.000106551, -0.00014757, -0.000187864, -0.000224554, -0.000254303, -0.000273425, -0.000278045, -0.000264315, -0.000228679, -0.000168179, -8.08036e-05, 3.41486e-05, 0.000175703, 0.000340864, 0.000524324, 0.000718252, 0.000912209, 0.0010932, 0.00124595, 0.0013533, 0.00139701, 0.00135862, 0.00122066, 0.00096808, 0.000589855, 8.06696e-05, -0.000557307, -0.00131275, -0.00216368, -0.00307639, -0.00400478, -0.00489047, -0.00566368, -0.00624517, -0.00654919, -0.00648766, -0.00597554, -0.00493719, -0.00331382, -0.00107147, 0.0017906, 0.00523181, 0.00916883, 0.0136017, 0.0186136, 0.0242453, 0.030496, 0.0373317, 0.0446936, 0.0525062, 0.0606842, 0.0691398, 0.077787, 0.0865467, 0.0953492, 0.104136, 0.112861, 0.12149, 0.130002, 0.138386, 0.146638, 0.154765, 0.162777, 0.170689, 0.178519, 0.186286, 0.194007, 0.2017, 0.20938, 0.217061, 0.224751, 0.23246, 0.240193, 0.247952, 0.255738, 0.263552, 0.27139, 0.279251, 0.287129, 0.295022, 0.302926, 0.310837, 0.318751, 0.326666, 0.33458, 0.34249, 0.350395, 0.358295, 0.366189, 0.374077, 0.38196, 0.389839, 0.397713, 0.405583, 0.413452, 0.421319, 0.429184, 0.43705, 0.444915, 0.452782, 0.460649, 0.468517, 0.476386, 0.484257, 0.492129, 0.500002 }
            };

            static ep_f64 easing_int(ep_u64 ease, ep_f64 p) noexcept {
                if (ease == 0 || ease > 29) ease = 1;
                return getIntegralValue(intTable[ease - 1], ktIntegralTableSize, p);
            }
        };
    };

    struct Rizline {
        using Official = Phigros::Official;
    };
};

namespace GL {
    /* !docs
    The OpenGL namespace.
    */

    using GLboolean = unsigned char;
    using GLbitfield = unsigned int;
    using GLbyte = signed char;
    using GLubyte = unsigned char;
    using GLshort = short;
    using GLushort = unsigned short;
    using GLint = int;
    using GLuint = unsigned int;
    using GLsizei = int;
    using GLfloat = float;
    using GLclampf = float;
    using GLdouble = double;
    using GLvoid = void;
    using GLenum = unsigned int;
    using GLsizeiptr = long long;
    using GLintptr = long long;
    using GLuint64 = uint64_t;
    using GLchar = signed char;
    using GLsync = struct __GLsync*;

    constexpr GLenum GL_NO_ERROR = 0;
    constexpr GLenum GL_INVALID_ENUM = 0x0500;
    constexpr GLenum GL_INVALID_VALUE = 0x0501;
    constexpr GLenum GL_INVALID_OPERATION = 0x0502;
    constexpr GLenum GL_OUT_OF_MEMORY = 0x0505;
    constexpr GLenum GL_VENDOR = 0x1F00;
    constexpr GLenum GL_RENDERER = 0x1F01;
    constexpr GLenum GL_VERSION = 0x1F02;
    constexpr GLenum GL_EXTENSIONS = 0x1F03;
    constexpr GLenum GL_MAJOR_VERSION = 0x821B;
    constexpr GLenum GL_MINOR_VERSION = 0x821C;
    constexpr GLenum GL_MAX_TEXTURE_SIZE = 0x0D33;
    constexpr GLenum GL_MAX_VERTEX_ATTRIBS = 0x8869;
    constexpr GLenum GL_MAX_TEXTURE_IMAGE_UNITS = 0x8872;
    constexpr GLenum GL_MAX_DRAW_BUFFERS = 0x8824;
    constexpr GLenum GL_MAX_UNIFORM_BLOCK_SIZE = 0x8A30;
    constexpr GLenum GL_MAX_VERTEX_UNIFORM_BLOCKS = 0x8A2B;
    constexpr GLenum GL_MAX_FRAGMENT_UNIFORM_BLOCKS = 0x8A2D;
    constexpr GLenum GL_VIEWPORT = 0x0BA2;
    constexpr GLenum GL_SCISSOR_BOX = 0x0C10;
    constexpr GLenum GL_SCISSOR_TEST = 0x0C11;
    constexpr GLenum GL_BLEND = 0x0BE2;
    constexpr GLenum GL_DEPTH_TEST = 0x0B71;
    constexpr GLenum GL_STENCIL_TEST = 0x0B90;
    constexpr GLenum GL_CULL_FACE = 0x0B44;
    constexpr GLenum GL_DITHER = 0x0BD0;
    constexpr GLenum GL_COLOR_CLEAR_VALUE = 0x0C22;
    constexpr GLenum GL_UNPACK_ALIGNMENT = 0x0CF5;
    constexpr GLenum GL_PACK_ALIGNMENT = 0x0D05;
    constexpr GLenum GL_FRAMEBUFFER_BINDING = 0x8CA6;
    constexpr GLenum GL_READ_FRAMEBUFFER_BINDING = 0x8CAA;
    constexpr GLenum GL_DRAW_FRAMEBUFFER_BINDING = 0x8CA6;
    constexpr GLenum GL_ARRAY_BUFFER_BINDING = 0x8894;
    constexpr GLenum GL_RENDERBUFFER_BINDING = 0x8CA7;
    constexpr GLenum GL_CURRENT_PROGRAM = 0x8B8D;
    constexpr GLenum GL_TEXTURE_BINDING_2D = 0x8069;
    constexpr GLenum GL_COLOR_WRITEMASK = 0x0C23;
    constexpr GLenum GL_DEPTH_WRITEMASK = 0x0B72;
    constexpr GLenum GL_SAMPLES = 0x80A9;

    constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
    constexpr GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
    constexpr GLenum GL_UNIFORM_BUFFER = 0x8A11;
    constexpr GLenum GL_PIXEL_PACK_BUFFER = 0x88EB;
    constexpr GLenum GL_PIXEL_UNPACK_BUFFER = 0x88EC;

    constexpr GLenum GL_STREAM_DRAW = 0x88E0;
    constexpr GLenum GL_STREAM_READ = 0x88E1;
    constexpr GLenum GL_STREAM_COPY = 0x88E2;
    constexpr GLenum GL_STATIC_DRAW = 0x88E4;
    constexpr GLenum GL_STATIC_READ = 0x88E5;
    constexpr GLenum GL_STATIC_COPY = 0x88E6;
    constexpr GLenum GL_DYNAMIC_DRAW = 0x88E8;
    constexpr GLenum GL_DYNAMIC_READ = 0x88E9;
    constexpr GLenum GL_DYNAMIC_COPY = 0x88EA;

    constexpr GLenum GL_READ_ONLY = 0x88B8;
    constexpr GLenum GL_WRITE_ONLY = 0x88B9;
    constexpr GLenum GL_READ_WRITE = 0x88BA;

    constexpr GLbitfield GL_MAP_READ_BIT = 0x0001;
    constexpr GLbitfield GL_MAP_WRITE_BIT = 0x0002;
    constexpr GLbitfield GL_MAP_INVALIDATE_RANGE_BIT = 0x0004;
    constexpr GLbitfield GL_MAP_INVALIDATE_BUFFER_BIT = 0x0008;
    constexpr GLbitfield GL_MAP_FLUSH_EXPLICIT_BIT = 0x0010;
    constexpr GLbitfield GL_MAP_UNSYNCHRONIZED_BIT = 0x0020;

    constexpr GLenum GL_BYTE = 0x1400;
    constexpr GLenum GL_SHORT = 0x1402;
    constexpr GLenum GL_INT = 0x1404;
    constexpr GLenum GL_HALF_FLOAT = 0x140B;
    constexpr GLenum GL_FIXED = 0x140C;
    constexpr GLenum GL_DOUBLE = 0x140A;

    constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
    constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;

    constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
    constexpr GLenum GL_LINK_STATUS = 0x8B82;
    constexpr GLenum GL_INFO_LOG_LENGTH = 0x8B84;
    constexpr GLenum GL_DELETE_STATUS = 0x8B80;
    constexpr GLenum GL_SHADER_TYPE = 0x8B4F;

    constexpr GLenum GL_ACTIVE_ATTRIBUTES = 0x8B89;
    constexpr GLenum GL_ACTIVE_UNIFORMS = 0x8B86;
    constexpr GLenum GL_ACTIVE_ATTRIBUTE_MAX_LENGTH = 0x8B8A;
    constexpr GLenum GL_ACTIVE_UNIFORM_MAX_LENGTH = 0x8B87;

    constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
    constexpr GLenum GL_TEXTURE_CUBE_MAP = 0x8513;
    constexpr GLenum GL_TEXTURE_CUBE_MAP_POSITIVE_X = 0x8515;
    constexpr GLenum GL_TEXTURE_CUBE_MAP_NEGATIVE_X = 0x8516;
    constexpr GLenum GL_TEXTURE_CUBE_MAP_POSITIVE_Y = 0x8517;
    constexpr GLenum GL_TEXTURE_CUBE_MAP_NEGATIVE_Y = 0x8518;
    constexpr GLenum GL_TEXTURE_CUBE_MAP_POSITIVE_Z = 0x8519;
    constexpr GLenum GL_TEXTURE_CUBE_MAP_NEGATIVE_Z = 0x851A;
    constexpr GLenum GL_TEXTURE_2D_MULTISAMPLE = 0x9100;

    constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
    constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
    constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
    constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
    constexpr GLenum GL_NEAREST = 0x2600;
    constexpr GLenum GL_LINEAR = 0x2601;
    constexpr GLenum GL_NEAREST_MIPMAP_NEAREST = 0x2700;
    constexpr GLenum GL_LINEAR_MIPMAP_NEAREST = 0x2701;
    constexpr GLenum GL_NEAREST_MIPMAP_LINEAR = 0x2702;
    constexpr GLenum GL_LINEAR_MIPMAP_LINEAR = 0x2703;
    constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
    constexpr GLenum GL_REPEAT = 0x2901;
    constexpr GLenum GL_MIRRORED_REPEAT = 0x8370;

    constexpr GLenum GL_TEXTURE0 = 0x84C0;

    constexpr GLenum GL_RED = 0x1903;
    constexpr GLenum GL_RG = 0x8227;
    constexpr GLenum GL_RGB = 0x1907;
    constexpr GLenum GL_RGBA = 0x1908;
    constexpr GLenum GL_BGR = 0x80E0;
    constexpr GLenum GL_BGRA = 0x80E1;
    constexpr GLenum GL_R8 = 0x8229;
    constexpr GLenum GL_RG8 = 0x822B;
    constexpr GLenum GL_RGB8 = 0x8051;
    constexpr GLenum GL_RGBA8 = 0x8058;
    constexpr GLenum GL_R16F = 0x822D;
    constexpr GLenum GL_RG16F = 0x822F;
    constexpr GLenum GL_RGB16F = 0x881B;
    constexpr GLenum GL_RGBA16F = 0x881A;
    constexpr GLenum GL_R32F = 0x822E;
    constexpr GLenum GL_RG32F = 0x8230;
    constexpr GLenum GL_RGB32F = 0x8815;
    constexpr GLenum GL_RGBA32F = 0x8814;
    constexpr GLenum GL_RGB10_A2 = 0x8059;
    constexpr GLenum GL_DEPTH_COMPONENT = 0x1902;
    constexpr GLenum GL_DEPTH_COMPONENT16 = 0x81A5;
    constexpr GLenum GL_DEPTH_COMPONENT24 = 0x81A6;
    constexpr GLenum GL_DEPTH_COMPONENT32F = 0x8CAC;
    constexpr GLenum GL_DEPTH24_STENCIL8 = 0x88F0;
    constexpr GLenum GL_FLOAT = 0x1406;
    constexpr GLenum GL_UNSIGNED_INT_24_8 = 0x84FA;

    constexpr GLenum GL_FRAMEBUFFER = 0x8D40;
    constexpr GLenum GL_READ_FRAMEBUFFER = 0x8CA8;
    constexpr GLenum GL_DRAW_FRAMEBUFFER = 0x8CA9;
    constexpr GLenum GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
    constexpr GLenum GL_COLOR_ATTACHMENT0 = 0x8CE0;
    constexpr GLenum GL_COLOR_ATTACHMENT1 = 0x8CE1;
    constexpr GLenum GL_COLOR_ATTACHMENT2 = 0x8CE2;
    constexpr GLenum GL_COLOR_ATTACHMENT3 = 0x8CE3;
    constexpr GLenum GL_DEPTH_ATTACHMENT = 0x8D00;
    constexpr GLenum GL_STENCIL_ATTACHMENT = 0x8D20;
    constexpr GLenum GL_DEPTH_STENCIL_ATTACHMENT = 0x821A;

    constexpr GLenum GL_RENDERBUFFER = 0x8D41;

    constexpr GLenum GL_POINTS = 0x0000;
    constexpr GLenum GL_LINES = 0x0001;
    constexpr GLenum GL_LINE_LOOP = 0x0002;
    constexpr GLenum GL_LINE_STRIP = 0x0003;
    constexpr GLenum GL_TRIANGLES = 0x0004;
    constexpr GLenum GL_TRIANGLE_STRIP = 0x0005;
    constexpr GLenum GL_TRIANGLE_FAN = 0x0006;

    constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;
    constexpr GLenum GL_UNSIGNED_SHORT = 0x1403;
    constexpr GLenum GL_UNSIGNED_INT = 0x1405;

    constexpr GLenum GL_COLOR_BUFFER_BIT = 0x00004000;
    constexpr GLenum GL_DEPTH_BUFFER_BIT = 0x00000100;
    constexpr GLenum GL_STENCIL_BUFFER_BIT = 0x00000400;

    constexpr GLenum GL_ZERO = 0;
    constexpr GLenum GL_ONE = 1;
    constexpr GLenum GL_SRC_COLOR = 0x0300;
    constexpr GLenum GL_ONE_MINUS_SRC_COLOR = 0x0301;
    constexpr GLenum GL_SRC_ALPHA = 0x0302;
    constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;
    constexpr GLenum GL_DST_ALPHA = 0x0304;
    constexpr GLenum GL_ONE_MINUS_DST_ALPHA = 0x0305;
    constexpr GLenum GL_DST_COLOR = 0x0306;
    constexpr GLenum GL_ONE_MINUS_DST_COLOR = 0x0307;

    constexpr GLenum GL_FUNC_ADD = 0x8006;
    constexpr GLenum GL_FUNC_SUBTRACT = 0x800A;
    constexpr GLenum GL_FUNC_REVERSE_SUBTRACT = 0x800B;
    constexpr GLenum GL_MIN = 0x8007;
    constexpr GLenum GL_MAX = 0x8008;

    constexpr GLenum GL_NEVER = 0x0200;
    constexpr GLenum GL_LESS = 0x0201;
    constexpr GLenum GL_EQUAL = 0x0202;
    constexpr GLenum GL_LEQUAL = 0x0203;
    constexpr GLenum GL_GREATER = 0x0204;
    constexpr GLenum GL_NOTEQUAL = 0x0205;
    constexpr GLenum GL_GEQUAL = 0x0206;
    constexpr GLenum GL_ALWAYS = 0x0207;

    constexpr GLenum GL_KEEP = 0x1E00;
    constexpr GLenum GL_REPLACE = 0x1E01;
    constexpr GLenum GL_INCR = 0x1E02;
    constexpr GLenum GL_DECR = 0x1E03;
    constexpr GLenum GL_INVERT = 0x150A;
    constexpr GLenum GL_INCR_WRAP = 0x8507;
    constexpr GLenum GL_DECR_WRAP = 0x8508;

    constexpr GLenum GL_TIME_ELAPSED = 0x88BF;
    constexpr GLenum GL_QUERY_RESULT = 0x8866;
    constexpr GLenum GL_QUERY_RESULT_AVAILABLE = 0x8867;

    constexpr GLenum GL_SYNC_GPU_COMMANDS_COMPLETE = 0x9117;
    constexpr GLenum GL_ALREADY_SIGNALED = 0x911A;
    constexpr GLenum GL_TIMEOUT_EXPIRED = 0x911B;
    constexpr GLenum GL_CONDITION_SATISFIED = 0x911C;
    constexpr GLenum GL_WAIT_FAILED = 0x911D;
    constexpr GLenum GL_SYNC_FLUSH_COMMANDS_BIT = 0x00000001;
    constexpr GLuint64 GL_TIMEOUT_IGNORED = 0xFFFFFFFFFFFFFFFFull;
    constexpr GLenum GL_SYNC_STATUS = 0x9114;
    constexpr GLenum GL_SIGNALED = 0x9119;

    constexpr GLint GL_TRUE = 1;
    constexpr GLint GL_FALSE = 0;

    constexpr GLenum GL_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FE;
    constexpr GLenum GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FF;
    constexpr GLenum GL_TEXTURE_REDUCTION_MODE_EXT = 0x9366;
    constexpr GLenum GL_NUM_EXTENSIONS = 0x821D;

    struct GL33CoreInterface {
        /* !docs
        A struct containing pointers to the OpenGL 3.3 core functions.
        */

        GLenum (*glGetError)();
        void (*glGetIntegerv)(GLenum pname, GLint* data);
        void (*glGetFloatv)(GLenum pname, GLfloat* data);
        void (*glGetBooleanv)(GLenum pname, GLboolean* data);
        const GLubyte* (*glGetString)(GLenum name);
        const GLubyte* (*glGetStringi)(GLenum name, GLuint index);
        void (*glEnable)(GLenum cap);
        void (*glDisable)(GLenum cap);
        GLboolean (*glIsEnabled)(GLenum cap);
        void (*glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
        void (*glScissor)(GLint x, GLint y, GLsizei width, GLsizei height);
        void (*glClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
        void (*glClear)(GLbitfield mask);
        void (*glPixelStorei)(GLenum pname, GLint param);
        void (*glFlush)();
        void (*glFinish)();

        void (*glGenBuffers)(GLsizei n, GLuint* buffers);
        void (*glDeleteBuffers)(GLsizei n, const GLuint* buffers);
        void (*glBindBuffer)(GLenum target, GLuint buffer);
        void (*glBufferData)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
        void (*glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
        void* (*glMapBuffer)(GLenum target, GLenum access);
        void* (*glMapBufferRange)(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
        GLboolean (*glUnmapBuffer)(GLenum target);
        void (*glBindBufferRange)(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
        void (*glBindBufferBase)(GLenum target, GLuint index, GLuint buffer);

        void (*glGenVertexArrays)(GLsizei n, GLuint* arrays);
        void (*glDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
        void (*glBindVertexArray)(GLuint array);
        void (*glEnableVertexAttribArray)(GLuint index);
        void (*glDisableVertexAttribArray)(GLuint index);
        void (*glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
        void (*glVertexAttribIPointer)(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer);
        void (*glVertexAttribDivisor)(GLuint index, GLuint divisor);

        GLuint (*glCreateShader)(GLenum type);
        void (*glShaderSource)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
        void (*glCompileShader)(GLuint shader);
        void (*glGetShaderiv)(GLuint shader, GLenum pname, GLint* params);
        void (*glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
        void (*glDeleteShader)(GLuint shader);
        GLuint (*glCreateProgram)();
        void (*glAttachShader)(GLuint program, GLuint shader);
        void (*glDetachShader)(GLuint program, GLuint shader);
        void (*glLinkProgram)(GLuint program);
        void (*glUseProgram)(GLuint program);
        void (*glGetProgramiv)(GLuint program, GLenum pname, GLint* params);
        void (*glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
        void (*glDeleteProgram)(GLuint program);
        void (*glGetActiveAttrib)(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
        void (*glGetActiveUniform)(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name);
        GLint (*glGetAttribLocation)(GLuint program, const GLchar* name);
        GLint (*glGetUniformLocation)(GLuint program, const GLchar* name);

        void (*glUniform1f)(GLint location, GLfloat v0);
        void (*glUniform2f)(GLint location, GLfloat v0, GLfloat v1);
        void (*glUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
        void (*glUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
        void (*glUniform1i)(GLint location, GLint v0);
        void (*glUniform2i)(GLint location, GLint v0, GLint v1);
        void (*glUniform3i)(GLint location, GLint v0, GLint v1, GLint v2);
        void (*glUniform4i)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
        void (*glUniform1fv)(GLint location, GLsizei count, const GLfloat* value);
        void (*glUniform2fv)(GLint location, GLsizei count, const GLfloat* value);
        void (*glUniform3fv)(GLint location, GLsizei count, const GLfloat* value);
        void (*glUniform4fv)(GLint location, GLsizei count, const GLfloat* value);
        void (*glUniform1iv)(GLint location, GLsizei count, const GLint* value);
        void (*glUniform2iv)(GLint location, GLsizei count, const GLint* value);
        void (*glUniform3iv)(GLint location, GLsizei count, const GLint* value);
        void (*glUniform4iv)(GLint location, GLsizei count, const GLint* value);
        void (*glUniformMatrix2fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
        void (*glUniformMatrix3fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
        void (*glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);

        void (*glGenTextures)(GLsizei n, GLuint* textures);
        void (*glDeleteTextures)(GLsizei n, const GLuint* textures);
        void (*glBindTexture)(GLenum target, GLuint texture);
        void (*glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
        void (*glTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
        void (*glTexParameteri)(GLenum target, GLenum pname, GLint param);
        void (*glTexParameterf)(GLenum target, GLenum pname, GLfloat param);
        void (*glGenerateMipmap)(GLenum target);
        void (*glActiveTexture)(GLenum texture);
        void (*glTexStorage2D)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
        void (*glTexImage2DMultisample)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);

        void (*glGenFramebuffers)(GLsizei n, GLuint* framebuffers);
        void (*glDeleteFramebuffers)(GLsizei n, const GLuint* framebuffers);
        void (*glBindFramebuffer)(GLenum target, GLuint framebuffer);
        GLenum (*glCheckFramebufferStatus)(GLenum target);
        void (*glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
        void (*glBlitFramebuffer)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);

        void (*glGenRenderbuffers)(GLsizei n, GLuint* renderbuffers);
        void (*glDeleteRenderbuffers)(GLsizei n, const GLuint* renderbuffers);
        void (*glBindRenderbuffer)(GLenum target, GLuint renderbuffer);
        void (*glRenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
        void (*glFramebufferRenderbuffer)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);

        void (*glDrawArrays)(GLenum mode, GLint first, GLsizei count);
        void (*glDrawElements)(GLenum mode, GLsizei count, GLenum type, const void* indices);
        void (*glDrawArraysInstanced)(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
        void (*glDrawElementsInstanced)(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount);

        void (*glBlendFunc)(GLenum sfactor, GLenum dfactor);
        void (*glBlendFuncSeparate)(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
        void (*glBlendEquation)(GLenum mode);
        void (*glBlendEquationSeparate)(GLenum modeRGB, GLenum modeAlpha);
        void (*glBlendColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
        void (*glDepthFunc)(GLenum func);
        void (*glDepthMask)(GLboolean flag);
        void (*glStencilFunc)(GLenum func, GLint ref, GLuint mask);
        void (*glStencilOp)(GLenum sfail, GLenum dpfail, GLenum dppass);
        void (*glStencilMask)(GLuint mask);
        void (*glColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);

        void (*glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels);
        void (*glCopyTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
        void (*glReadBuffer)(GLenum src);
        void (*glDrawBuffer)(GLenum dst);

        void (*glGenQueries)(GLsizei n, GLuint* ids);
        void (*glDeleteQueries)(GLsizei n, const GLuint* ids);
        void (*glBeginQuery)(GLenum target, GLuint id);
        void (*glEndQuery)(GLenum target);
        void (*glGetQueryObjectiv)(GLuint id, GLenum pname, GLint* params);
        void (*glGetQueryObjectui64v)(GLuint id, GLenum pname, GLuint64* params);

        GLsync (*glFenceSync)(GLenum condition, GLbitfield flags);
        void (*glDeleteSync)(GLsync sync);
        GLenum (*glClientWaitSync)(GLsync sync, GLbitfield flags, GLuint64 timeout);
        void (*glGetSynciv)(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei* length, GLint* values);

        bool isExtensionSupported(const char* extension) const noexcept {
            /* !docs
            Checks if the given OpenGL extension is supported.
            */

            GLint num;
            glGetIntegerv(GL_NUM_EXTENSIONS, &num);

            for (GLint i = 0; i < num; i++) {
                auto* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
                if (strcmp(ext, extension) == 0) {
                    return true;
                }
            }

            return false;
        }
    };

    using GLProcLoader = std::function<void*(const char*)>;
    static GL33CoreInterface MakeGL33CoreInterface(GLProcLoader loader) {
        /* !docs
        Creates a interface object from a procedure loader.
        */

        GL33CoreInterface interface {};

        #define LOAD_AND_CHECK(proc) { \
            auto ptr = loader(#proc); \
            if (!ptr) { \
                throw std::runtime_error("failed to load " #proc); \
            } \
            using F = decltype(interface.proc); \
            interface.proc = (F)ptr; \
        }

        LOAD_AND_CHECK(glGetError)
        LOAD_AND_CHECK(glGetIntegerv)
        LOAD_AND_CHECK(glGetFloatv)
        LOAD_AND_CHECK(glGetBooleanv)
        LOAD_AND_CHECK(glGetString)
        LOAD_AND_CHECK(glGetStringi)
        LOAD_AND_CHECK(glEnable)
        LOAD_AND_CHECK(glDisable)
        LOAD_AND_CHECK(glIsEnabled)
        LOAD_AND_CHECK(glViewport)
        LOAD_AND_CHECK(glScissor)
        LOAD_AND_CHECK(glClearColor)
        LOAD_AND_CHECK(glClear)
        LOAD_AND_CHECK(glPixelStorei)
        LOAD_AND_CHECK(glFlush)
        LOAD_AND_CHECK(glFinish)

        LOAD_AND_CHECK(glGenBuffers)
        LOAD_AND_CHECK(glDeleteBuffers)
        LOAD_AND_CHECK(glBindBuffer)
        LOAD_AND_CHECK(glBufferData)
        LOAD_AND_CHECK(glBufferSubData)
        LOAD_AND_CHECK(glMapBuffer)
        LOAD_AND_CHECK(glMapBufferRange)
        LOAD_AND_CHECK(glUnmapBuffer)
        LOAD_AND_CHECK(glBindBufferRange)
        LOAD_AND_CHECK(glBindBufferBase)

        LOAD_AND_CHECK(glGenVertexArrays)
        LOAD_AND_CHECK(glDeleteVertexArrays)
        LOAD_AND_CHECK(glBindVertexArray)
        LOAD_AND_CHECK(glEnableVertexAttribArray)
        LOAD_AND_CHECK(glDisableVertexAttribArray)
        LOAD_AND_CHECK(glVertexAttribPointer)
        LOAD_AND_CHECK(glVertexAttribIPointer)
        LOAD_AND_CHECK(glVertexAttribDivisor)

        LOAD_AND_CHECK(glCreateShader)
        LOAD_AND_CHECK(glShaderSource)
        LOAD_AND_CHECK(glCompileShader)
        LOAD_AND_CHECK(glGetShaderiv)
        LOAD_AND_CHECK(glGetShaderInfoLog)
        LOAD_AND_CHECK(glDeleteShader)
        LOAD_AND_CHECK(glCreateProgram)
        LOAD_AND_CHECK(glAttachShader)
        LOAD_AND_CHECK(glDetachShader)
        LOAD_AND_CHECK(glLinkProgram)
        LOAD_AND_CHECK(glUseProgram)
        LOAD_AND_CHECK(glGetProgramiv)
        LOAD_AND_CHECK(glGetProgramInfoLog)
        LOAD_AND_CHECK(glDeleteProgram)
        LOAD_AND_CHECK(glGetActiveAttrib)
        LOAD_AND_CHECK(glGetActiveUniform)
        LOAD_AND_CHECK(glGetAttribLocation)
        LOAD_AND_CHECK(glGetUniformLocation)

        LOAD_AND_CHECK(glUniform1f)
        LOAD_AND_CHECK(glUniform2f)
        LOAD_AND_CHECK(glUniform3f)
        LOAD_AND_CHECK(glUniform4f)
        LOAD_AND_CHECK(glUniform1i)
        LOAD_AND_CHECK(glUniform2i)
        LOAD_AND_CHECK(glUniform3i)
        LOAD_AND_CHECK(glUniform4i)
        LOAD_AND_CHECK(glUniform1fv)
        LOAD_AND_CHECK(glUniform2fv)
        LOAD_AND_CHECK(glUniform3fv)
        LOAD_AND_CHECK(glUniform4fv)
        LOAD_AND_CHECK(glUniform1iv)
        LOAD_AND_CHECK(glUniform2iv)
        LOAD_AND_CHECK(glUniform3iv)
        LOAD_AND_CHECK(glUniform4iv)
        LOAD_AND_CHECK(glUniformMatrix2fv)
        LOAD_AND_CHECK(glUniformMatrix3fv)
        LOAD_AND_CHECK(glUniformMatrix4fv)

        LOAD_AND_CHECK(glGenTextures)
        LOAD_AND_CHECK(glDeleteTextures)
        LOAD_AND_CHECK(glBindTexture)
        LOAD_AND_CHECK(glTexImage2D)
        LOAD_AND_CHECK(glTexSubImage2D)
        LOAD_AND_CHECK(glTexParameteri)
        LOAD_AND_CHECK(glTexParameterf)
        LOAD_AND_CHECK(glGenerateMipmap)
        LOAD_AND_CHECK(glActiveTexture)
        LOAD_AND_CHECK(glTexStorage2D)
        LOAD_AND_CHECK(glTexImage2DMultisample)

        LOAD_AND_CHECK(glGenFramebuffers)
        LOAD_AND_CHECK(glDeleteFramebuffers)
        LOAD_AND_CHECK(glBindFramebuffer)
        LOAD_AND_CHECK(glCheckFramebufferStatus)
        LOAD_AND_CHECK(glFramebufferTexture2D)
        LOAD_AND_CHECK(glBlitFramebuffer)

        LOAD_AND_CHECK(glGenRenderbuffers)
        LOAD_AND_CHECK(glDeleteRenderbuffers)
        LOAD_AND_CHECK(glBindRenderbuffer)
        LOAD_AND_CHECK(glRenderbufferStorage)
        LOAD_AND_CHECK(glFramebufferRenderbuffer)

        LOAD_AND_CHECK(glDrawArrays)
        LOAD_AND_CHECK(glDrawElements)
        LOAD_AND_CHECK(glDrawArraysInstanced)
        LOAD_AND_CHECK(glDrawElementsInstanced)

        LOAD_AND_CHECK(glBlendFunc)
        LOAD_AND_CHECK(glBlendFuncSeparate)
        LOAD_AND_CHECK(glBlendEquation)
        LOAD_AND_CHECK(glBlendEquationSeparate)
        LOAD_AND_CHECK(glBlendColor)
        LOAD_AND_CHECK(glDepthFunc)
        LOAD_AND_CHECK(glDepthMask)
        LOAD_AND_CHECK(glStencilFunc)
        LOAD_AND_CHECK(glStencilOp)
        LOAD_AND_CHECK(glStencilMask)
        LOAD_AND_CHECK(glColorMask)

        LOAD_AND_CHECK(glReadPixels)
        LOAD_AND_CHECK(glCopyTexSubImage2D)
        LOAD_AND_CHECK(glReadBuffer)
        LOAD_AND_CHECK(glDrawBuffer)

        LOAD_AND_CHECK(glGenQueries)
        LOAD_AND_CHECK(glDeleteQueries)
        LOAD_AND_CHECK(glBeginQuery)
        LOAD_AND_CHECK(glEndQuery)
        LOAD_AND_CHECK(glGetQueryObjectiv)
        LOAD_AND_CHECK(glGetQueryObjectui64v)

        LOAD_AND_CHECK(glFenceSync)
        LOAD_AND_CHECK(glDeleteSync)
        LOAD_AND_CHECK(glClientWaitSync)
        LOAD_AND_CHECK(glGetSynciv)

        return interface;
    }
    
    struct GLvec2 {
        GLfloat x, y;
        
        GLvec2() : x(0), y(0) {}
        GLvec2(const Vec2& o) : x(o.x), y(o.y) {}
        template <typename A, typename B> constexpr GLvec2(A a, B b) : x((GLfloat)a), y((GLfloat)b) {}

        GLvec2 operator+(const GLvec2& o) const noexcept { return {x + o.x, y + o.y}; }
        GLvec2 operator-(const GLvec2& o) const noexcept { return {x - o.x, y - o.y}; }
        GLvec2 operator*(const GLvec2& o) const noexcept { return {x * o.x, y * o.y}; }
        GLvec2 operator/(const GLvec2& o) const noexcept { return {x / o.x, y / o.y}; }
        GLvec2 operator+(GLfloat o) const noexcept { return {x + o, y + o}; }
        GLvec2 operator-(GLfloat o) const noexcept { return {x - o, y - o}; }
        GLvec2 operator*(GLfloat o) const noexcept { return {x * o, y * o}; }
        GLvec2 operator/(GLfloat o) const noexcept { return {x / o, y / o}; }
        GLvec2 operator-() const noexcept { return {-x, -y}; }
        GLvec2& operator+=(const GLvec2& o) noexcept { x += o.x; y += o.y; return *this; }
        GLvec2& operator-=(const GLvec2& o) noexcept { x -= o.x; y -= o.y; return *this; }
        GLvec2& operator*=(const GLvec2& o) noexcept { x *= o.x; y *= o.y; return *this; }
        GLvec2& operator/=(const GLvec2& o) noexcept { x /= o.x; y /= o.y; return *this; }
        GLvec2& operator+=(GLfloat o) noexcept { x += o; y += o; return *this; }
        GLvec2& operator-=(GLfloat o) noexcept { x -= o; y -= o; return *this; }
        GLvec2& operator*=(GLfloat o) noexcept { x *= o; y *= o; return *this; }
        GLvec2& operator/=(GLfloat o) noexcept { x /= o; y /= o; return *this; }
        bool operator==(const GLvec2& o) const noexcept { return x == o.x && y == o.y; }
        bool operator!=(const GLvec2& o) const noexcept { return x != o.x || y != o.y; }
    };
    static_assert(offsetof(GLvec2, x) == 0, "GLvec2.x must be at offset 0");
    static_assert(offsetof(GLvec2, y) == sizeof(GLfloat), "GLvec2.y must be at offset 1");

    struct GLvec3 {
        GLfloat x, y, z;
        
        GLvec3() : x(0), y(0), z(0) {}
        template <typename A, typename B, typename C> constexpr GLvec3(A a, B b, C c) : x((GLfloat)a), y((GLfloat)b), z((GLfloat)c) {}

        GLvec3 operator+(const GLvec3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
        GLvec3 operator-(const GLvec3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
        GLvec3 operator*(const GLvec3& o) const noexcept { return {x * o.x, y * o.y, z * o.z}; }
        GLvec3 operator/(const GLvec3& o) const noexcept { return {x / o.x, y / o.y, z / o.z}; }
        GLvec3 operator+(GLfloat o) const noexcept { return {x + o, y + o, z + o}; }
        GLvec3 operator-(GLfloat o) const noexcept { return {x - o, y - o, z - o}; }
        GLvec3 operator*(GLfloat o) const noexcept { return {x * o, y * o, z * o}; }
        GLvec3 operator/(GLfloat o) const noexcept { return {x / o, y / o, z / o}; }
        GLvec3 operator-() const noexcept { return {-x, -y, -z}; }
        GLvec3& operator+=(const GLvec3& o) noexcept { x += o.x; y += o.y; z += o.z; return *this; }
        GLvec3& operator-=(const GLvec3& o) noexcept { x -= o.x; y -= o.y; z -= o.z; return *this; }
        GLvec3& operator*=(const GLvec3& o) noexcept { x *= o.x; y *= o.y; z *= o.z; return *this; }
        GLvec3& operator/=(const GLvec3& o) noexcept { x /= o.x; y /= o.y; z /= o.z; return *this; }
        GLvec3& operator+=(GLfloat o) noexcept { x += o; y += o; z += o; return *this; }
        GLvec3& operator-=(GLfloat o) noexcept { x -= o; y -= o; z -= o; return *this; }
        GLvec3& operator*=(GLfloat o) noexcept { x *= o; y *= o; z *= o; return *this; }
        GLvec3& operator/=(GLfloat o) noexcept { x /= o; y /= o; z /= o; return *this; }
        bool operator==(const GLvec3& o) const noexcept { return x == o.x && y == o.y && z == o.z; }
        bool operator!=(const GLvec3& o) const noexcept { return x != o.x || y != o.y || z != o.z; }
    };
    static_assert(offsetof(GLvec3, x) == 0, "GLvec3.x must be at offset 0");
    static_assert(offsetof(GLvec3, y) == sizeof(GLfloat), "GLvec3.y must be at offset 1");
    static_assert(offsetof(GLvec3, z) == sizeof(GLfloat) * 2, "GLvec3.z must be at offset 2");

    struct GLvec4 {
        GLfloat x, y, z, w;
        
        GLvec4() : x(0), y(0), z(0), w(0) {}
        GLvec4(const Color& o) : x(o.r), y(o.g), z(o.b), w(o.a) {}
        template <typename A, typename B, typename C, typename D> constexpr GLvec4(A a, B b, C c, D d) : x((GLfloat)a), y((GLfloat)b), z((GLfloat)c), w((GLfloat)d) {}

        GLvec4 operator+(const GLvec4& o) const noexcept { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
        GLvec4 operator-(const GLvec4& o) const noexcept { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
        GLvec4 operator*(const GLvec4& o) const noexcept { return {x * o.x, y * o.y, z * o.z, w * o.w}; }
        GLvec4 operator/(const GLvec4& o) const noexcept { return {x / o.x, y / o.y, z / o.z, w / o.w}; }
        GLvec4 operator+(GLfloat o) const noexcept { return {x + o, y + o, z + o, w + o}; }
        GLvec4 operator-(GLfloat o) const noexcept { return {x - o, y - o, z - o, w - o}; }
        GLvec4 operator*(GLfloat o) const noexcept { return {x * o, y * o, z * o, w * o}; }
        GLvec4 operator/(GLfloat o) const noexcept { return {x / o, y / o, z / o, w / o}; }
        GLvec4 operator-() const noexcept { return {-x, -y, -z, -w}; }
        GLvec4& operator+=(const GLvec4& o) noexcept { x += o.x; y += o.y; z += o.z; w += o.w; return *this; }
        GLvec4& operator-=(const GLvec4& o) noexcept { x -= o.x; y -= o.y; z -= o.z; w -= o.w; return *this; }
        GLvec4& operator*=(const GLvec4& o) noexcept { x *= o.x; y *= o.y; z *= o.z; w *= o.w; return *this; }
        GLvec4& operator/=(const GLvec4& o) noexcept { x /= o.x; y /= o.y; z /= o.z; w /= o.w; return *this; }
        GLvec4& operator+=(GLfloat o) noexcept { x += o; y += o; z += o; w += o; return *this; }
        GLvec4& operator-=(GLfloat o) noexcept { x -= o; y -= o; z -= o; w -= o; return *this; }
        GLvec4& operator*=(GLfloat o) noexcept { x *= o; y *= o; z *= o; w *= o; return *this; }
        GLvec4& operator/=(GLfloat o) noexcept { x /= o; y /= o; z /= o; w /= o; return *this; }
        bool operator==(const GLvec4& o) const noexcept { return x == o.x && y == o.y && z == o.z && w == o.w; }
        bool operator!=(const GLvec4& o) const noexcept { return x != o.x || y != o.y || z != o.z || w != o.w; }

        static GLvec4 White() noexcept { return { 1.0, 1.0, 1.0, 1.0 }; }
        static GLvec4 Black() noexcept { return { 0.0, 0.0, 0.0, 1.0 }; }
        static GLvec4 Red() noexcept { return { 1.0, 0.0, 0.0, 1.0 }; }
        static GLvec4 Green() noexcept { return { 0.0, 1.0, 0.0, 1.0 }; }
        static GLvec4 Blue() noexcept { return { 0.0, 0.0, 1.0, 1.0 }; }
        static GLvec4 Transparent() noexcept { return { 0.0, 0.0, 0.0, 0.0 }; }
        static GLvec4 Gray(GLfloat v = 0.5) noexcept { return { v, v, v, 1.0 }; }
    };
    static_assert(offsetof(GLvec4, x) == 0, "GLvec4.x must be at offset 0");
    static_assert(offsetof(GLvec4, y) == sizeof(GLfloat), "GLvec4.y must be at offset 1");
    static_assert(offsetof(GLvec4, z) == sizeof(GLfloat) * 2, "GLvec4.z must be at offset 2");
    static_assert(offsetof(GLvec4, w) == sizeof(GLfloat) * 3, "GLvec4.w must be at offset 3");

    struct Vertex {
        /* !docs
        A vertex, with position and texture coordinates.
        */

        GLvec2 position;
        GLvec2 texCoord;
    };

    struct BufferInfo;
    struct VertexArrayInfo;
    struct ShaderInfo;
    struct ProgramInfo;
    struct TextureInfo;
    struct FramebufferInfo;
    struct RenderbufferInfo;
    struct QueryInfo;
    struct SyncInfo;

    struct VertexPool {
        /* !docs
        A pool of vertices, which can be allocated from.
        */

        struct Chunk {
            std::vector<Vertex> vertices;
            ep_u64 offset;

            static ep_sp<Chunk> Make(ep_u64 size) {
                auto* ck = new Chunk();
                ck->vertices.resize(size);
                return ep_sp<Chunk>(ck);
            }

            Vertex* alloc(ep_u64 count) noexcept {
                if (offset + count > vertices.size()) return nullptr;
                auto* ret = &vertices[offset];
                offset += count;
                return ret;
            }
        };

        static constexpr ep_u64 defaultChunkSize = 4096;
        static constexpr ep_u64 defaultChunkCount = 4;

        std::vector<ep_sp<Chunk>> chunks;
        ep_u64 offset;

        static ep_sp<VertexPool> Make() {
            auto* vp = new VertexPool();
            vp->chunks.resize(defaultChunkCount);
            for (ep_u64 i = 0; i < defaultChunkCount; i++) {
                vp->chunks[i] = Chunk::Make(defaultChunkSize);
            }
            return ep_sp<VertexPool>(vp);
        }

        struct AllocResult {
            /* !docs
            The result of allocating vertices from a `@../VertexPool`.
            */

            Vertex* vertices;
            ep_u64 count;
            ep_u64 sig;
            ep_u64 offset;

            Vertex* next() noexcept {
                ep_assert(offset < count, "VertexPool::AllocResult::next() called after end of allocation");
                return vertices + (offset++);
            }

            void reset() noexcept {
                offset = 0;
            }

            Vertex* begin() noexcept { return vertices; }
            Vertex* end() noexcept { return vertices + count; }
            const Vertex* begin() const noexcept { return vertices; }
            const Vertex* end() const noexcept { return vertices + count; }
        };

        AllocResult alloc(ep_u64 count) noexcept {
            while (offset < chunks.size()) {
                auto& chunk = chunks[offset];
                auto* ptr = chunk->alloc(count);
                if (ptr) return allocSuccess(ptr, count);
                offset++;
            }

            chunks.push_back(Chunk::Make(std::max(defaultChunkSize, count)));
            return allocSuccess(chunks[offset]->alloc(count), count);
        }

        void reset() {
            /* !docs
            Resets the vertex pool, freeing all allocated vertices.
            */

            offset = 0;
            for (auto& chunk : chunks) chunk->offset = 0;
            sig++;
        }

        bool valid(const AllocResult& result) const noexcept {
            return result.sig == sig;
        }

        private:
        ep_u64 sig;

        AllocResult allocSuccess(Vertex* ptr, ep_u64 count) {
            return {
                .vertices = ptr,
                .count = count,
                .sig = sig
            };
        }
    };

    struct Mesh {
        /* !docs
        A mesh of vertices, which can be drawn.
        */

        VertexPool::AllocResult vertices;
        GLvec4 color;
        TextureInfo* texture;
        ProgramInfo* program;

        void addRect(const GLvec2& position, const GLvec2& size, const GLvec2& uvPosition, const GLvec2& uvSize) noexcept {
            if (size.x <= 0 || size.y <= 0) return;

            *vertices.next() = { position, uvPosition };
            *vertices.next() = { position + GLvec2 { size.x, 0 }, uvPosition + GLvec2 { uvSize.x, 0 } };
            *vertices.next() = { position + size, uvPosition + uvSize };

            *vertices.next() = { position, uvPosition };
            *vertices.next() = { position + GLvec2 { 0, size.y }, uvPosition + GLvec2 { 0, uvSize.y } };
            *vertices.next() = { position + size, uvPosition + uvSize };
        }

        void addFullRect() noexcept {
            /* !docs
            Adds a full-screen rectangle to the mesh.
            */

            addRect({ -1, -1 }, { 2, 2 }, { 0, 0 }, { 1, 1 });
        }

        static ep_u64 getPolygonVerticesCount(ep_u64 pointsCount) noexcept {
            /* !docs
            Get the number of vertices required to draw a polygon with the given number of points.
            */

            return (pointsCount - 2) * 3;
        }

        void addPolygon(const std::vector<GLvec2>& points, const std::vector<GLvec2>& uvs) noexcept {
            /* !docs
            Adds a polygon to the mesh.
            */

            ep_assert(uvs.size() >= points.size(), "Not enough UVs for polygon");

            for (ep_i64 i = 0; i < (ep_i64)points.size() - 2; i++) {
                *vertices.next() = { points[0], uvs[0] };
                *vertices.next() = { points[i + 1], uvs[i + 1] };
                *vertices.next() = { points[i + 2], uvs[i + 2] };
            }
        }
    };

    struct BufferInfo {
        /* !docs
        A buffer object, which is created by `glGenBuffers`.
        */
        
        BufferInfo() = default;
        BufferInfo(const BufferInfo&) = delete;
        BufferInfo& operator=(const BufferInfo&) = delete;

        BufferInfo(BufferInfo&& other) noexcept
            : glRef(other.glRef)
            , id(std::exchange(other.id, 0))
            , target(other.target)
        {}

        BufferInfo& operator=(BufferInfo&& other) noexcept {
            if (this != &other) {
                reset();
                glRef = other.glRef;
                id = std::exchange(other.id, 0);
                target = other.target;
            }

            return *this;
        }

        GL33CoreInterface* glRef;

        GLuint id;
        GLenum target;

        struct UsingGuard {
            BufferInfo* ref;

            UsingGuard(BufferInfo& buffer) : ref(&buffer) {
                ref->glRef->glBindBuffer(ref->target, ref->id);
            }

            UsingGuard(const UsingGuard&) = delete;
            UsingGuard& operator=(const UsingGuard&) = delete;
            UsingGuard(UsingGuard&&) = delete;
            UsingGuard& operator=(UsingGuard&&) = delete;

            void data(GLsizeiptr size, const void* data, GLenum usage) {
                ref->glRef->glBufferData(ref->target, size, data, usage);
            }

            template <typename T>
            void data(std::span<const T> data, GLenum usage) {
                data(ref->target, data.size() * sizeof(T), data.data(), usage);
            }

            void subData(GLintptr offset, GLsizeiptr size, const void* data) {
                ref->glRef->glBufferSubData(ref->target, offset, size, data);
            }

            template <typename T>
            void subData(GLintptr offset, std::span<const T> data) {
                ref->glRef->glBufferSubData(ref->target, offset, data.size() * sizeof(T), data.data());
            }

            struct MappingGuard {
                UsingGuard* ref;
                void* data;

                MappingGuard(UsingGuard& buffer, GLbitfield access = GL_READ_WRITE) : ref(&buffer) {
                    data = ref->ref->glRef->glMapBuffer(ref->ref->target, access);
                }

                MappingGuard(const MappingGuard&) = delete;
                MappingGuard& operator=(const MappingGuard&) = delete;
                MappingGuard(MappingGuard&&) = delete;
                MappingGuard& operator=(MappingGuard&&) = delete;

                ~MappingGuard() {
                    ref->ref->glRef->glUnmapBuffer(ref->ref->target);
                }
            };

            struct RangeMappingGuard {
                UsingGuard* ref;
                void* data;

                RangeMappingGuard(UsingGuard& buffer, GLintptr offset, GLsizeiptr length, GLbitfield access = GL_READ_WRITE) : ref(&buffer) {
                    data = ref->ref->glRef->glMapBufferRange(ref->ref->target, offset, length, access);
                }

                RangeMappingGuard(const RangeMappingGuard&) = delete;
                RangeMappingGuard& operator=(const RangeMappingGuard&) = delete;
                RangeMappingGuard(RangeMappingGuard&&) = delete;
                RangeMappingGuard& operator=(RangeMappingGuard&&) = delete;

                ~RangeMappingGuard() {
                    ref->ref->glRef->glUnmapBuffer(ref->ref->target);
                }
            };

            MappingGuard map(GLbitfield access) {
                return MappingGuard(*this, access);
            }

            ep_sp<MappingGuard> mapSp(GLbitfield access) {
                auto* guard = new MappingGuard(*this, access);
                return ep_sp<MappingGuard>(guard);
            }

            RangeMappingGuard mapRange(GLintptr offset, GLsizeiptr length, GLbitfield access) {
                return RangeMappingGuard(*this, offset, length, access);
            }

            ep_sp<RangeMappingGuard> mapRangeSp(GLintptr offset, GLsizeiptr length, GLbitfield access) {
                auto* guard = new RangeMappingGuard(*this, offset, length, access);
                return ep_sp<RangeMappingGuard>(guard);
            }

            ~UsingGuard() {
                ref->glRef->glBindBuffer(ref->target, 0);
            }
        };

        UsingGuard use() {
            return UsingGuard(*this);
        }

        ep_sp<UsingGuard> useSp() {
            auto* guard = new UsingGuard(*this);
            return ep_sp<UsingGuard>(guard);
        }

        ~BufferInfo() {
            reset();
        }

        private:
        void reset() {
            if (id) {
                glRef->glDeleteBuffers(1, &id);
                id = 0;
            }
        }
    };

    struct VertexArrayInfo {
        /* !docs
        A vertex array object (VAO), which is created by `glGenVertexArrays`.
        */

        VertexArrayInfo() = default;
        VertexArrayInfo(const VertexArrayInfo&) = delete;
        VertexArrayInfo& operator=(const VertexArrayInfo&) = delete;

        VertexArrayInfo(VertexArrayInfo&& other) noexcept
            : glRef(other.glRef)
            , id(std::exchange(other.id, 0))
        {}

        VertexArrayInfo& operator=(VertexArrayInfo&& other) noexcept {
            if (this != &other) {
                reset();
                glRef = other.glRef;
                id = std::exchange(other.id, 0);
            }

            return *this;
        }

        GL33CoreInterface* glRef;

        GLuint id;

        struct UsingGuard {
            VertexArrayInfo* ref;

            UsingGuard(VertexArrayInfo& array) : ref(&array) {
                ref->glRef->glBindVertexArray(ref->id);
            }

            UsingGuard(const UsingGuard&) = delete;
            UsingGuard& operator=(const UsingGuard&) = delete;
            UsingGuard(UsingGuard&&) = delete;
            UsingGuard& operator=(UsingGuard&&) = delete;

            void enable(GLuint index) { ref->glRef->glEnableVertexAttribArray(index); }
            void disable(GLuint index) { ref->glRef->glDisableVertexAttribArray(index); }

            void pointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) {
                ref->glRef->glVertexAttribPointer(index, size, type, normalized, stride, pointer);
            }

            void iPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer) {
                ref->glRef->glVertexAttribIPointer(index, size, type, stride, pointer);
            }

            void divisor(GLuint index, GLuint divisor) {
                ref->glRef->glVertexAttribDivisor(index, divisor);
            }

            ~UsingGuard() {
                // ref->glRef->glBindVertexArray(0);
            }
        };

        UsingGuard use() {
            return UsingGuard(*this);
        }

        ~VertexArrayInfo() {
            reset();
        }

        private:
        void reset() {
            if (id) {
                glRef->glDeleteVertexArrays(1, &id);
                id = 0;
            }
        }
    };

    struct ShaderInfo {
        /* !docs
        A shader object, which is created by `glCreateShader`.
        */

        ShaderInfo() = default;
        ShaderInfo(const ShaderInfo&) = delete;
        ShaderInfo& operator=(const ShaderInfo&) = delete;

        ShaderInfo(ShaderInfo&& other) noexcept
            : glRef(other.glRef)
            , type(other.type)
            , id(std::exchange(other.id, 0))
        {}

        ShaderInfo& operator=(ShaderInfo&& other) noexcept {
            if (this != &other) {
                reset();
                glRef = other.glRef;
                type = other.type;
                id = std::exchange(other.id, 0);
            }

            return *this;
        }

        GL33CoreInterface* glRef;

        GLenum type;
        GLuint id;

        void source(const std::string& source) {
            const GLchar* ptr = (GLchar*)source.c_str();
            GLint len = source.length();
            glRef->glShaderSource(id, 1, &ptr, &len);
        }

        void source(std::span<const std::string> sources) {
            std::vector<const GLchar*> ptrs;
            std::vector<GLint> lens;
            ptrs.reserve(sources.size());
            lens.reserve(sources.size());
            for (const auto& source : sources) {
                ptrs.push_back((GLchar*)source.c_str());
                lens.push_back(source.length());
            }
            glRef->glShaderSource(id, sources.size(), ptrs.data(), lens.data());
        }

        bool compile(std::string* outLog = nullptr) {
            glRef->glCompileShader(id);
            GLint status = 0;
            glRef->glGetShaderiv(id, GL_COMPILE_STATUS, &status);
            if (outLog) {
                GLint logLen;
                glRef->glGetShaderiv(id, GL_INFO_LOG_LENGTH, &logLen);
                if (logLen > 0) {
                    outLog->resize(logLen);
                    GLsizei written = 0;
                    glRef->glGetShaderInfoLog(id, logLen, &written, (GLchar*)outLog->data());
                    outLog->resize(written);
                }
            }
            return status == GL_TRUE;
        }

        ~ShaderInfo() {
            reset();
        }

        private:
        void reset() {
            if (id) {
                glRef->glDeleteShader(id);
                id = 0;
            }
        }
    };

    struct VertexLayout {
        /* !docs
        A vertex layout, which is used to describe the structure of the vertex data and store the vertex buffer.
        It includes a vertex array object (VAO) and a vertex buffer object (VBO).
        */

        ep_sp<VertexArrayInfo> vao;
        ep_sp<BufferInfo> vbo;
    };

    struct VertexLayoutPool {
        /* !docs
        A vertex layout pool.
        */

        std::vector<VertexLayout> layouts;
        ep_u64 currentIndex;
        ep_u64 frameSig;

        using LayoutCreator = std::function<VertexLayout()>;
        void resize(ep_u64 size,const LayoutCreator& creator) {
            layouts.resize(size);
            for (auto& layout : layouts) layout = creator();
        }

        using ConfigureFunc = std::function<void(VertexArrayInfo*, BufferInfo*)>;
        void configure(const ConfigureFunc& func) {
            for (auto& layout : layouts) {
                func(layout.vao.get(), layout.vbo.get());
            }
        }

        void checkAndNext(ep_u64 newFrameSig) noexcept {
            if (frameSig != newFrameSig) {
                frameSig = newFrameSig;
                currentIndex = (currentIndex + 1) % layouts.size();
            }
        }

        VertexLayout& current() noexcept {
            return layouts[currentIndex];
        }
    };

    struct ProgramInfo {
        /* !docs
        A program object, which is created by `glCreateProgram`.
        */

        ProgramInfo() = default;
        ProgramInfo(const ProgramInfo&) = delete;
        ProgramInfo& operator=(const ProgramInfo&) = delete;

        ProgramInfo(ProgramInfo&& other) noexcept
            : glRef(other.glRef)
            , id(std::exchange(other.id, 0))
            , vertexLayoutPool(std::move(other.vertexLayoutPool))
            , bufferFiller(other.bufferFiller)
            , fragConfig(std::move(other.fragConfig))
            , attribLocations(std::move(other.attribLocations))
            , uniformLocations(std::move(other.uniformLocations))
        {}

        ProgramInfo& operator=(ProgramInfo&& other) noexcept {
            if (this != &other) {
                reset();
                glRef = other.glRef;
                id = std::exchange(other.id, 0);
                vertexLayoutPool = std::move(other.vertexLayoutPool);
                bufferFiller = other.bufferFiller;
                fragConfig = std::move(other.fragConfig);
                attribLocations = std::move(other.attribLocations);
                uniformLocations = std::move(other.uniformLocations);
            }

            return *this;
        }

        GL33CoreInterface* glRef;

        GLuint id;

        VertexLayoutPool vertexLayoutPool;

        using BufferFillerFunc = std::function<void(ProgramInfo*, const VertexLayout&, VertexPool::AllocResult&)>;
        BufferFillerFunc bufferFiller;

        struct {
            std::string textureUniformName = "uTexture";
            std::optional<std::string> colorUniformName = "uColor";
        } fragConfig;

        void attachShader(ShaderInfo* shader) {
            glRef->glAttachShader(id, shader->id);
            attribLocations.clear();
            uniformLocations.clear();
        }

        bool link(std::string* outLog = nullptr) {
            glRef->glLinkProgram(id);
            GLint status = 0;
            glRef->glGetProgramiv(id, GL_LINK_STATUS, &status);
            if (outLog) {
                GLint logLen = 0;
                glRef->glGetProgramiv(id, GL_INFO_LOG_LENGTH, &logLen);
                if (logLen > 0) {
                    outLog->resize(logLen);
                    GLsizei written = 0;
                    glRef->glGetProgramInfoLog(id, logLen, &written, (GLchar*)outLog->data());
                    outLog->resize(written);
                }
            }
            return status == GL_TRUE;
        }

        struct UsingGuard {
            ProgramInfo* ref;

            UsingGuard(ProgramInfo& program) : ref(&program) {
                ref->glRef->glUseProgram(ref->id);
            }

            UsingGuard(const UsingGuard&) = delete;
            UsingGuard& operator=(const UsingGuard&) = delete;
            UsingGuard(UsingGuard&&) = delete;
            UsingGuard& operator=(UsingGuard&&) = delete;

            ~UsingGuard() {
                // ref->glRef->glUseProgram(0);
            }
        };

        UsingGuard use() {
            return UsingGuard(*this);
        }

        struct Location {
            ProgramInfo* ref;

            GLint location;

            void setf(GLfloat v0) noexcept { ref->glRef->glUniform1f(location, v0); }
            void setf(GLfloat v0, GLfloat v1) noexcept { ref->glRef->glUniform2f(location, v0, v1); }
            void setf(GLfloat v0, GLfloat v1, GLfloat v2) noexcept { ref->glRef->glUniform3f(location, v0, v1, v2); }
            void setf(GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) noexcept { ref->glRef->glUniform4f(location, v0, v1, v2, v3); }
            
            void seti(GLint v0) noexcept { ref->glRef->glUniform1i(location, v0); }
            void seti(GLint v0, GLint v1) noexcept { ref->glRef->glUniform2i(location, v0, v1); }
            void seti(GLint v0, GLint v1, GLint v2) noexcept { ref->glRef->glUniform3i(location, v0, v1, v2); }
            void seti(GLint v0, GLint v1, GLint v2, GLint v3) noexcept { ref->glRef->glUniform4i(location, v0, v1, v2, v3); }

            void setv2(const GLvec2& value) noexcept { setf(value.x, value.y); }
            void setv3(const GLvec3& value) noexcept { setf(value.x, value.y, value.z); }
            void setv4(const GLvec4& value) noexcept { setf(value.x, value.y, value.z, value.w); }

            void setMatrix2fv(GLsizei count, GLboolean transpose, const GLfloat* value) noexcept { ref->glRef->glUniformMatrix2fv(location, count, transpose, value); }
            void setMatrix3fv(GLsizei count, GLboolean transpose, const GLfloat* value) noexcept { ref->glRef->glUniformMatrix3fv(location, count, transpose, value); }
            void setMatrix4fv(GLsizei count, GLboolean transpose, const GLfloat* value) noexcept { ref->glRef->glUniformMatrix4fv(location, count, transpose, value); }
        };

        GLint getAttribLocationPosition(const std::string& name) noexcept {
            if (attribLocations.find(name) == attribLocations.end()) {
                attribLocations[name] = glRef->glGetAttribLocation(id, (GLchar*)name.c_str());
            }

            return attribLocations[name];
        }

        GLint getUniformLocationPosition(const std::string& name) noexcept {
            if (uniformLocations.find(name) == uniformLocations.end()) {
                uniformLocations[name] = glRef->glGetUniformLocation(id, (GLchar*)name.c_str());
            }

            return uniformLocations[name];
        }

        Location getAttribLocation(const std::string& name) noexcept {
            Location result;
            result.ref = this;
            result.location = getAttribLocationPosition(name);
            return result;
        }

        Location getUniformLocation(const std::string& name) noexcept {
            Location result;
            result.ref = this;
            result.location = getUniformLocationPosition(name);
            return result;
        }

        void setVertices(VertexPool::AllocResult& vertices) {
            if (!bufferFiller) return;
            auto& vertexLayout = vertexLayoutPool.current();
            bufferFiller(this, vertexLayout, vertices);
        }

        ~ProgramInfo() {
            reset();
        }

        private:
        std::unordered_map<std::string, GLint> attribLocations;
        std::unordered_map<std::string, GLint> uniformLocations;

        void reset() {
            if (id) {
                glRef->glDeleteProgram(id);
                id = 0;
            }
        }
    };

    struct TextureInfo {
        /* !docs
        A texture object, which is created by `glGenTextures`.
        */

        TextureInfo() = default;
        TextureInfo(const TextureInfo&) = delete;
        TextureInfo& operator=(const TextureInfo&) = delete;

        TextureInfo(TextureInfo&& other) noexcept
            : glRef(other.glRef)
            , target(other.target)
            , id(std::exchange(other.id, 0))
            , width(other.width) , height(other.height)
            , frameBuffer(std::move(other.frameBuffer))
            , pingPong(std::move(other.pingPong))
        {}

        TextureInfo& operator=(TextureInfo&& other) noexcept {
            if (this != &other) {
                reset();
                glRef = other.glRef;
                target = other.target;
                id = std::exchange(other.id, 0);
                width = other.width; height = other.height;
                frameBuffer = std::move(other.frameBuffer);
                pingPong = std::move(other.pingPong);
            }

            return *this;
        }

        GL33CoreInterface* glRef;

        GLenum target;
        GLuint id;
        
        GLsizei width, height;
        ep_sp<FramebufferInfo> frameBuffer;
        ep_sp<TextureInfo> pingPong;

        struct UsingGuard {
            TextureInfo* ref;

            GLenum index;

            UsingGuard(TextureInfo& texture, GLenum index) : ref(&texture) {
                this->index = index;
                ref->glRef->glActiveTexture(GL_TEXTURE0 + index);
                ref->glRef->glBindTexture(ref->target, ref->id);
            }

            // only GL_RGBA and GL_UNSIGNED_BYTE ;)
            void image2D(GLsizei width, GLsizei height, const void* pixels) {
                if (width <= 0 || height <= 0) return;

                ref->glRef->glTexImage2D(ref->target, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

                GLfloat maxAniso = 0.0;
                ref->glRef->glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
                if (maxAniso > 0.0) ref->glRef->glTexParameterf(ref->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);

                ref->glRef->glTexParameteri(ref->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                ref->glRef->glTexParameteri(ref->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                ref->glRef->glTexParameteri(ref->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                ref->glRef->glTexParameteri(ref->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                ref->width = width; ref->height = height;
            }

            void image2D(const DecodedRGBATexture& decoded) {
                image2D(decoded.width, decoded.height, (void*)decoded.data.data());
            }

            void storage2D(GLint levels, GLenum internalformat, GLsizei width, GLsizei height) {
                ref->glRef->glTexStorage2D(ref->target, levels, internalformat, width, height);
            }

            void image2DMultisample(GLsizei width, GLsizei height, GLsizei samples) {
                ref->glRef->glTexImage2DMultisample(ref->target, samples, GL_RGBA8, width, height, GL_TRUE);
                ref->width = width; ref->height = height;
            }

            ~UsingGuard() {
                ref->glRef->glBindTexture(ref->target, 0);
            }
        };

        UsingGuard use(GLenum index = 0) {
            return UsingGuard(*this, index);
        }

        bool sizeIsSame(TextureInfo* other) const noexcept {
            return other->width == width && other->height == height;
        }

        bool sizeIsSame(GLsizei width, GLsizei height) const noexcept {
            return this->width == width && this->height == height;
        }

        GLvec2 size() const noexcept {
            return { width, height };
        }

        ~TextureInfo() {
            reset();
        }

        private:
        void reset() {
            if (id) {
                glRef->glDeleteTextures(1, &id);
                id = 0;
            }
        }
    };

    struct FramebufferInfo {
        /* !docs
        A framebuffer object (FBO), which is created by `glGenFramebuffers`.
        */

        FramebufferInfo() = default;
        FramebufferInfo(const FramebufferInfo&) = delete;
        FramebufferInfo& operator=(const FramebufferInfo&) = delete;

        FramebufferInfo(FramebufferInfo&& other) noexcept
            : glRef(other.glRef)
            , id(std::exchange(other.id, 0))
        {}

        FramebufferInfo& operator=(FramebufferInfo&& other) noexcept {
            if (this != &other) {
                reset();
                glRef = other.glRef;
                id = std::exchange(other.id, 0);
            }

            return *this;
        }

        GL33CoreInterface* glRef;

        GLuint id;

        struct UsingGuard {
            FramebufferInfo* ref;
            GLenum target;
            GLint savedId;

            UsingGuard(FramebufferInfo& framebuffer, TextureInfo* texture, GLenum target) : ref(&framebuffer), target(target) {
                ref->glRef->glGetIntegerv(target == GL_READ_FRAMEBUFFER ? GL_READ_FRAMEBUFFER_BINDING : (target == GL_DRAW_FRAMEBUFFER ? GL_DRAW_FRAMEBUFFER_BINDING : GL_FRAMEBUFFER_BINDING), &savedId);
                ref->glRef->glBindFramebuffer(target, ref->id);
                ref->glRef->glFramebufferTexture2D(target, GL_COLOR_ATTACHMENT0, texture->target, texture->id, 0);
            }

            ~UsingGuard() {
                ref->glRef->glBindFramebuffer(target, savedId);
            }
        };

        UsingGuard use(TextureInfo* texture, GLenum target) {
            return UsingGuard(*this, texture, target);
        }

        ep_sp<UsingGuard> useSp(TextureInfo* texture, GLenum target) {
            auto* guard = new UsingGuard(*this, texture, target);
            return ep_sp<UsingGuard>(guard);
        }

        ~FramebufferInfo() {
            reset();
        }

        private:
        void reset() {
            if (id) {
                glRef->glDeleteFramebuffers(1, &id);
                id = 0;
            }
        }
    };

    struct RenderbufferInfo {
        /* !docs
        A renderbuffer object (RBO), which is created by `glGenRenderbuffers`.
        */

        RenderbufferInfo() = default;
        RenderbufferInfo(const RenderbufferInfo&) = delete;
        RenderbufferInfo& operator=(const RenderbufferInfo&) = delete;

        RenderbufferInfo(RenderbufferInfo&& other) noexcept
            : glRef(other.glRef)
            , id(std::exchange(other.id, 0))
        {}

        RenderbufferInfo& operator=(RenderbufferInfo&& other) noexcept {
            if (this != &other) {
                reset();
                glRef = other.glRef;
                id = std::exchange(other.id, 0);
            }

            return *this;
        }

        GL33CoreInterface* glRef;

        GLuint id;

        struct UsingGuard {
            RenderbufferInfo* ref;
            GLint savedId;

            UsingGuard(RenderbufferInfo& renderbuffer) : ref(&renderbuffer) {
                ref->glRef->glGetIntegerv(GL_RENDERBUFFER_BINDING, &savedId);
                ref->glRef->glBindRenderbuffer(GL_RENDERBUFFER, ref->id);
            }

            void storage(GLenum internalformat, GLsizei width, GLsizei height) {
                ref->glRef->glRenderbufferStorage(GL_RENDERBUFFER, internalformat, width, height);
            }

            ~UsingGuard() {
                ref->glRef->glBindRenderbuffer(GL_RENDERBUFFER, savedId);
            }
        };

        UsingGuard use() {
            return UsingGuard(*this);
        }

        ~RenderbufferInfo() {
            reset();
        }

        private:
        void reset() {
            if (id) {
                glRef->glDeleteRenderbuffers(1, &id);
                id = 0;
            }
        }
    };

    struct QueryInfo {
        /* !docs
        A query object, which is created by `glGenQueries`.
        */

        QueryInfo() = default;
        QueryInfo(const QueryInfo&) = delete;
        QueryInfo& operator=(const QueryInfo&) = delete;

        QueryInfo(QueryInfo&& other) noexcept
            : glRef(other.glRef)
            , id(std::exchange(other.id, 0))
        {}

        QueryInfo& operator=(QueryInfo&& other) noexcept {
            if (this != &other) {
                reset();
                glRef = other.glRef;
                id = std::exchange(other.id, 0);
            }

            return *this;
        }

        GL33CoreInterface* glRef;

        GLuint id;

        struct UsingGuard {
            QueryInfo* ref;
            GLenum target;

            UsingGuard(QueryInfo& query, GLenum target) : ref(&query), target(target) {
                ref->glRef->glBeginQuery(target, ref->id);
            }

            ~UsingGuard() {
                ref->glRef->glEndQuery(target);
            }
        };

        UsingGuard use(GLenum target = GL_TIME_ELAPSED) {
            return UsingGuard(*this, target);
        }

        GLint getResultInt() const {
            GLint result = 0;
            glRef->glGetQueryObjectiv(id, GL_QUERY_RESULT, &result);
            return result;
        }

        GLuint64 getResultUInt64() const {
            GLuint64 result = 0;
            glRef->glGetQueryObjectui64v(id, GL_QUERY_RESULT, &result);
            return result;
        }

        bool isResultAvailable() const {
            GLint result = 0;
            glRef->glGetQueryObjectiv(id, GL_QUERY_RESULT_AVAILABLE, &result);
            return result != 0;
        }

        ~QueryInfo() {
            reset();
        }

        private:
        void reset() {
            if (id) {
                glRef->glDeleteQueries(1, &id);
                id = 0;
            }
        }
    };

    struct SyncInfo {
        /* !docs
        A sync object, which is created by `glFenceSync`.
        */

        SyncInfo() = default;
        SyncInfo(const SyncInfo&) = delete;
        SyncInfo& operator=(const SyncInfo&) = delete;

        SyncInfo(SyncInfo&& other) noexcept
            : glRef(other.glRef)
            , sync(std::exchange(other.sync, nullptr))
        {}

        SyncInfo& operator=(SyncInfo&& other) noexcept {
            if (this != &other) {
                reset();
                glRef = other.glRef;
                sync = std::exchange(other.sync, nullptr);
            }

            return *this;
        }

        GL33CoreInterface* glRef;

        GLsync sync;

        GLenum wait(GLbitfield flags, GLuint64 timeout = GL_TIMEOUT_IGNORED) const {
            return glRef->glClientWaitSync(sync, flags, timeout);
        }

        bool isSignaled() const noexcept {
            GLint status;
            glRef->glGetSynciv(sync, GL_SYNC_STATUS, 1, nullptr, &status);
            return status == GL_SIGNALED;
        }

        ~SyncInfo() {
            reset();
        }

        private:
        void reset() {
            if (sync) {
                glRef->glDeleteSync(sync);
                sync = nullptr;
            }
        }
    };

    static const char* defaultVertexShaderSource = R"(
#version 330 core

in vec2 inPosition;
in vec2 inTexCoord;

out vec2 fragTexCoord;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragTexCoord = inTexCoord;
}
)";

    static const char* defaultVertexShaderSource_RPE = R"(
#version 100

attribute vec2 inPosition;
attribute vec2 inTexCoord;

varying vec2 uv;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    uv = inTexCoord;
}
)";

    static const char* defaultFragmentShaderSource = R"(
#version 330 core

in vec2 fragTexCoord;

uniform vec4 uColor;
uniform sampler2D uTexture;

out vec4 outColor;

void main() {
    outColor = uColor * texture(uTexture, fragTexCoord);
}
)";

    struct GL33Context {
        /* !docs
        A gl context.
        */

        GL33Context() = default;
        GL33Context(const GL33Context&) = delete;
        GL33Context& operator=(const GL33Context&) = delete;
        GL33Context(GL33Context&&) = delete;
        GL33Context& operator=(GL33Context&&) = delete;

        GL33CoreInterface gl;

        ep_u64 drawCallsCount = 0;

        static ep_sp<GL33Context> Make(const GL33CoreInterface& interface) {
            auto* ctx = new GL33Context();
            ctx->gl = interface;
            ctx->initDefaultResources();
            return ep_sp<GL33Context>(ctx);
        }

        void enable(GLenum cap) noexcept { gl.glEnable(cap); }
        void disable(GLenum cap) noexcept { gl.glDisable(cap); }
        bool isEnabled(GLenum cap) const noexcept { return gl.glIsEnabled(cap); }

        struct GLFeatureGuard {
            GL33Context* glCtx;
            GLenum cap; bool enable;

            GLFeatureGuard(GL33Context* glCtx, GLenum cap, bool enable) : glCtx(glCtx), cap(cap), enable(enable) {}
            GLFeatureGuard(const GLFeatureGuard&) = delete;
            GLFeatureGuard& operator=(const GLFeatureGuard&) = delete;
            GLFeatureGuard(GLFeatureGuard&& other) = delete;
            GLFeatureGuard& operator=(GLFeatureGuard&& other) = delete;

            ~GLFeatureGuard() {
                if (enable && !glCtx->isEnabled(cap)) glCtx->enable(cap);
                else if (!enable && glCtx->isEnabled(cap)) glCtx->disable(cap);
            }
        };

        GLFeatureGuard getFeatureGuard(GLenum cap) noexcept {
            /* !docs
            Returns a guard that will keep the feature enabled or disabled when it goes out of scope.
            */

            return GLFeatureGuard(this, cap, isEnabled(cap));
        }

        void setViewport(GLint x, GLint y, GLsizei width, GLsizei height) noexcept {
            currentViewport = { x, y, width, height };
            gl.glViewport(x, y, width, height);
        }

        void setViewport(GLsizei width, GLsizei height) noexcept { setViewport(0, 0, width, height); }
        void setViewport(const GLvec2& xy, const GLvec2& wh) noexcept { setViewport(xy.x, xy.y, wh.x, wh.y); }
        void setViewport(const GLvec4& rect) noexcept { setViewport(rect.x, rect.y, rect.z, rect.w); }

        void setClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) noexcept { gl.glClearColor(r, g, b, a); }
        
        void clear(GLbitfield mask) noexcept { gl.glClear(mask); }

        void flush() { gl.glFlush(); }
        void finish() { gl.glFinish(); }

        ep_sp<BufferInfo> createBuffer(GLenum target = GL_ARRAY_BUFFER) {
            auto* info = new BufferInfo();
            info->glRef = &gl;
            info->target = target;
            gl.glGenBuffers(1, &info->id);
            return ep_sp<BufferInfo>(info);
        }

        ep_sp<VertexArrayInfo> createVertexArray() {
            auto* info = new VertexArrayInfo();
            info->glRef = &gl;
            gl.glGenVertexArrays(1, &info->id);
            return ep_sp<VertexArrayInfo>(info);
        }

        ep_sp<ShaderInfo> createShader(GLenum type) {
            auto* info = new ShaderInfo();
            info->glRef = &gl;
            info->type = type;
            info->id = gl.glCreateShader(type);
            return ep_sp<ShaderInfo>(info);
        }

        ep_sp<ProgramInfo> createProgram() {
            auto* info = new ProgramInfo();
            info->glRef = &gl;
            info->id = gl.glCreateProgram();
            return ep_sp<ProgramInfo>(info);
        }

        ep_sp<TextureInfo> createTexture(GLenum target = GL_TEXTURE_2D) {
            auto* info = new TextureInfo();
            info->glRef = &gl;
            info->target = target;
            gl.glGenTextures(1, &info->id);
            info->frameBuffer = createFramebuffer();
            return ep_sp<TextureInfo>(info);
        }

        ep_sp<FramebufferInfo> createFramebuffer() {
            auto* info = new FramebufferInfo();
            info->glRef = &gl;
            gl.glGenFramebuffers(1, &info->id);
            return ep_sp<FramebufferInfo>(info);
        }

        ep_sp<RenderbufferInfo> createRenderbuffer() {
            auto* info = new RenderbufferInfo();
            info->glRef = &gl;
            gl.glGenRenderbuffers(1, &info->id);
            return ep_sp<RenderbufferInfo>(info);
        }

        void blendFunc(GLenum sfactor, GLenum dfactor) noexcept { gl.glBlendFunc(sfactor, dfactor); }
        void blendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) noexcept { gl.glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha); }
        void blendEquation(GLenum mode) noexcept { gl.glBlendEquation(mode); }
        void blendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) noexcept { gl.glBlendEquationSeparate(modeRGB, modeAlpha); }
        void blendColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) noexcept { gl.glBlendColor(r, g, b, a); }
        
        void colorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) noexcept { gl.glColorMask(r, g, b, a); }

        ep_sp<QueryInfo> createQuery() {
            auto* info = new QueryInfo();
            info->glRef = &gl;
            gl.glGenQueries(1, &info->id);
            return ep_sp<QueryInfo>(info);
        }

        ep_sp<SyncInfo> createSync(GLenum condition = GL_SYNC_GPU_COMMANDS_COMPLETE, GLbitfield flags = 0) {
            auto* info = new SyncInfo();
            info->glRef = &gl;
            info->sync = gl.glFenceSync(condition, flags);
            return ep_sp<SyncInfo>(info);
        }

        ep_sp<ProgramInfo> createConfiguredProgram(const std::string& fragCode, const std::string& vertCode = defaultVertexShaderSource) {
            /* !docs
            Creates a configured program with default vertex shader and given fragment shader.
            */

            auto vert = createShader(GL_VERTEX_SHADER);
            auto frag = createShader(GL_FRAGMENT_SHADER);
            
            vert->source(vertCode);
            frag->source(fragCode);

            std::string log;
            if (!vert->compile(&log)) throw std::runtime_error("vertex compile: " + log);
            if (!frag->compile(&log)) throw std::runtime_error("fragment compile: " + log);

            auto prog = createProgram();
            prog->attachShader(vert.get());
            prog->attachShader(frag.get());
            if (!prog->link(&log)) throw std::runtime_error("program link: " + log);

            prog->vertexLayoutPool.resize(8, [&]() { return VertexLayout { .vao = createVertexArray(), .vbo = createBuffer() }; });
            prog->vertexLayoutPool.configure([&](VertexArrayInfo* vao, BufferInfo* vbo) {
                auto vaoGuard = vao->use();
                auto vboGuard = vbo->use();
                auto inPosition = prog->getAttribLocationPosition("inPosition");
                auto inTexCoord = prog->getAttribLocationPosition("inTexCoord");
                vaoGuard.enable(inPosition);
                vaoGuard.enable(inTexCoord);
                vaoGuard.pointer(inPosition, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
                vaoGuard.pointer(inTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
            });

            prog->bufferFiller = [](ProgramInfo* prog, const VertexLayout& vertexLayout, VertexPool::AllocResult& vertices) {
                auto vaoGuard = vertexLayout.vao->use();
                auto vboGuard = vertexLayout.vbo->use();
                vboGuard.data(
                    vertices.count * sizeof(Vertex),
                    vertices.vertices,
                    GL_DYNAMIC_DRAW
                );
            };

            return prog;
        }

        struct ProgramPresets {
            /* !docs
            A struct containing some preconfigured programs.
            */

            static ep_sp<ProgramInfo> gaussianBlur(GL33Context* glCtx) {
                return glCtx->createConfiguredProgram(R"(
#version 330 core

in vec2 fragTexCoord;

uniform vec4 uColor;
uniform sampler2D uTexture;

uniform vec2 uDelta; // (0, 1) or (1, 0)
uniform float uRadius; // 0.0 - 1.0
uniform int uIterations;
uniform bool uUseColor;

out vec4 outColor;

vec4 sampleTexture(vec2 texCoord) {
    return texture(uTexture, texCoord);
}

float gaussian(float x) {
    return exp(-x * x / 0.24);
}

void main() {
    vec4 sum = vec4(0.0);
    float wsum = 0.0;
    
    for (int i = -uIterations; i <= uIterations; i++) {
        float offset = float(i) / float(uIterations);
        float weight = gaussian(offset);
        sum += sampleTexture(fragTexCoord + uDelta * offset * uRadius) * weight;
        wsum += weight;
    }

    outColor = sum / wsum;
    outColor.a = 1.0;

    if (uUseColor) {
        outColor *= uColor;
    }
}
)");
            }

            static ep_sp<ProgramInfo> yuvConverter(GL33Context* glCtx) {
                auto prog = glCtx->createConfiguredProgram(R"(
#version 330 core

in vec2 fragTexCoord;

uniform sampler2D uTexture;
uniform ivec2 uResolution;
uniform bool uFlipY;

out vec4 outColor;

vec3 getPixel(int x, int y) {
    return texelFetch(uTexture, ivec2(x, uResolution.y - y - 1), 0).xyz;
}

float getY(int x, int y) {
    vec3 pixel = getPixel(x, y);
    return dot(pixel, vec3(0.299, 0.587, 0.114));
}

float getU(int x, int y) {
    vec3 pixel = (
        getPixel(x, y)
        + getPixel(x, y + 1)
        + getPixel(x + 1, y)
        + getPixel(x + 1, y + 1)
    ) * 0.25;
    return dot(pixel, vec3(-0.168736, -0.331264, 0.5)) + 0.5;
}

float getV(int x, int y) {
    vec3 pixel = (
        getPixel(x, y)
        + getPixel(x, y + 1)
        + getPixel(x + 1, y)
        + getPixel(x + 1, y + 1)
    ) * 0.25;
    return dot(pixel, vec3(0.5, -0.418688, -0.081312)) + 0.5;
}

float getYI(int index) {
    return getY(index % uResolution.x, index / uResolution.x);
}

float getUI(int index) {
    return getU((index % (uResolution.x / 2)) * 2, index / (uResolution.x / 2) * 2);
}

float getVI(int index) {
    return getV((index % (uResolution.x / 2)) * 2, index / (uResolution.x / 2) * 2);
}

void main() {
    int w = uResolution.x; int h = uResolution.y;
    ivec2 curr_pos = ivec2(fragTexCoord * vec2(uResolution));
    if (!uFlipY) curr_pos.y = h - curr_pos.y - 1;
    int byte_index = (int(curr_pos.x) + int(curr_pos.y) * w) * 4;

    int y_bytes = w * h; int uv_bytes = y_bytes / 4;

    if (byte_index < y_bytes) {
        int pixel_index = byte_index;
        outColor = vec4(
            getYI(pixel_index), getYI(pixel_index + 1),
            getYI(pixel_index + 2), getYI(pixel_index + 3)
        );
    } else if (byte_index < y_bytes + uv_bytes) {
        int pixel_index = byte_index - y_bytes;
        outColor = vec4(
            getUI(pixel_index), getUI(pixel_index + 1),
            getUI(pixel_index + 2), getUI(pixel_index + 3)
        );
    } else if (byte_index < y_bytes + uv_bytes * 2) {
        int pixel_index = byte_index - y_bytes - uv_bytes;
        outColor = vec4(
            getVI(pixel_index), getVI(pixel_index + 1),
            getVI(pixel_index + 2), getVI(pixel_index + 3)
        );
    } else outColor = vec4(0);
}
)");
                prog->fragConfig.colorUniformName = std::nullopt;
                return prog;
            }
        };

        void drawMesh(Mesh& mesh) noexcept {
            /* !docs
            Draw a mesh.
            */

            if (!vertexPool->valid(mesh.vertices)) {
                std::abort();
            }

            auto* prog = mesh.program ? mesh.program : defaultProgram.get();
            auto* tex = mesh.texture ? mesh.texture : defaultWhiteTexture.get();

            prog->vertexLayoutPool.checkAndNext(frameSig);
            prog->setVertices(mesh.vertices);

            auto progGuard = prog->use();
            auto& vertexLayout = prog->vertexLayoutPool.current();
            auto vaoGuard = vertexLayout.vao->use();
            auto texGuard = tex->use();

            if (prog->fragConfig.colorUniformName.has_value()) {
                prog->getUniformLocation(prog->fragConfig.colorUniformName.value()).setv4(mesh.color);
            }

            prog->getUniformLocation(prog->fragConfig.textureUniformName).seti(texGuard.index);

            gl.glDrawArrays(GL_TRIANGLES, 0, mesh.vertices.count);
            drawCallsCount++;
        }

        GLvec4 getViewport() const noexcept { return currentViewport; }

        struct ViewportGuard {
            GL33Context* glCtx;
            GLvec4 vp;

            ViewportGuard(GL33Context* glCtx, GLvec4 vp) : glCtx(glCtx), vp(vp) {}
            ViewportGuard(const ViewportGuard&) = delete;
            ViewportGuard& operator=(const ViewportGuard&) = delete;
            ViewportGuard(ViewportGuard&& other) = delete;
            ViewportGuard& operator=(ViewportGuard&& other) = delete;
            ~ViewportGuard() { glCtx->setViewport(vp); }
        };

        ViewportGuard getViewportGuard() noexcept {
            /* !docs
            Get a guard that will restore viewport on destruction.
            */

            return ViewportGuard(this, getViewport());
        }

        void copyTexture(TextureInfo* src, TextureInfo* dst) noexcept {
            /* !docs
            Copy texture to another texture.
            */

            if (!dst->sizeIsSame(src)) {
                dst->use().image2D(src->width, src->height, nullptr);
            }

            auto srcFbGuard = src->frameBuffer->use(src, GL_READ_FRAMEBUFFER);
            auto dstGuard = dst->use();

            gl.glCopyTexSubImage2D(
                GL_TEXTURE_2D, 0,
                0, 0,
                0, 0,
                src->width, src->height
            );
        }

        void copyCurrentToTexture(TextureInfo* dst) noexcept {
            /* !docs
            Copy current framebuffer to texture.
            It supports multisampling framebuffers.
            */

            auto kfboGuard = getFBOGuard();
            auto texFboGuard = dst->frameBuffer->use(dst, GL_DRAW_FRAMEBUFFER);
            gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, kfboGuard.drawFbo);
            gl.glBlitFramebuffer(
                0, 0, dst->width, dst->height,
                0, 0, dst->width, dst->height,
                GL_COLOR_BUFFER_BIT, GL_NEAREST
            );
        }

        ep_sp<TextureInfo> ensureTexturePingPong(TextureInfo* texture) noexcept {
            /* !docs
            Ensures that texture has a ping-pong texture and returns it.
            */

            if (!texture->pingPong) texture->pingPong = createTexture();
            copyTexture(texture, texture->pingPong.get());
            return texture->pingPong;
        }

        void renderIntoTexture(TextureInfo* texture, Mesh& descMesh) noexcept {
            /* !docs
            Render mesh into texture.
            */

            auto vpGuard = getViewportGuard();
            auto pingPong = ensureTexturePingPong(texture);
            auto fbGuard = texture->frameBuffer->use(texture, GL_DRAW_FRAMEBUFFER);
            auto feGuard = getFeatureGuard(GL_BLEND);

            setViewport(texture->width, texture->height);
            disable(GL_BLEND);

            descMesh.vertices.reset();
            descMesh.addFullRect();
            descMesh.texture = pingPong.get();
            drawMesh(descMesh);
        }

        void gaussianBlurToTexture(TextureInfo* texture, ep_f64 radius) {
            /* !docs
            Apply gaussian blur to texture.
            */

            auto mesh = requestMesh(6);
            mesh.program = preloadedPrograms.gaussianBlur.get();
            mesh.color = GLvec4::White();

            auto progGuard = mesh.program->use();
            mesh.program->getUniformLocation("uIterations").seti(std::ceil(radius / (1.0 + 0.15 * std::log2(radius + 1))));

            mesh.program->getUniformLocation("uDelta").setv2({ 0.0, 1.0 });
            mesh.program->getUniformLocation("uRadius").setf(radius / texture->height);
            mesh.program->getUniformLocation("uUseColor").seti(false);
            renderIntoTexture(texture, mesh);

            mesh.program->getUniformLocation("uDelta").setv2({ 1.0, 0.0 });
            mesh.program->getUniformLocation("uRadius").setf(radius / texture->width);
            mesh.program->getUniformLocation("uUseColor").seti(true);
            renderIntoTexture(texture, mesh);
        }

        void renderToDrawFbo(ep_u64 width, ep_u64 height, Mesh& descMesh) {
            /* !docs
            Render mesh into draw framebuffer.
            */

            auto tempTexGuard = allocTempTexture(width, height);
            auto tempTex = tempTexGuard.get();
            copyCurrentToTexture(tempTex.get());
            auto feGuard = getFeatureGuard(GL_BLEND);

            setViewport(width, height);
            disable(GL_BLEND);

            descMesh.vertices.reset();
            descMesh.addFullRect();
            descMesh.texture = tempTex.get();
            drawMesh(descMesh);
        }

        bool getCurrentIsMultiSampled() noexcept {
            /* !docs
            Check if current framebuffer is multisampled.
            */

            GLint samples;
            gl.glGetIntegerv(GL_SAMPLES, &samples);
            return samples > 1;
        }

        struct {
            ep_sp<ProgramInfo> gaussianBlur;
            ep_sp<ProgramInfo> yuvConverter;
        } preloadedPrograms; // !inline-docs| Preloaded programs.

        void frameEnded() noexcept {
            /* !docs
            Please call this function at the end of each frame.
            */

            drawCallsCount = 0;
            frameSig++;
        }

        Mesh requestMesh(ep_u64 verticesCount) noexcept {
            /* !docs
            Request mesh with specified number of vertices.
            */

            return {
                .vertices = vertexPool->alloc(verticesCount)
            };
        }

        struct AsyncFrameReader {
            /* !docs
            A frame reader which is used to read YUV420 frames from GPU.
            */

            GL33Context* glCtx;
            ep_u64 frameWidth, frameHeight;

            AsyncFrameReader() = default;
            AsyncFrameReader(const AsyncFrameReader&) = delete;
            AsyncFrameReader& operator=(const AsyncFrameReader&) = delete;
            AsyncFrameReader(AsyncFrameReader&& other) = default;
            AsyncFrameReader& operator=(AsyncFrameReader&& other) = default;

            void initBufferSlots(ep_u64 size) {
                for (ep_u64 i = 0; i < size; i++) addBufferSlot();
            }

            struct ReadResult {
                ep_u64 size;
                ep_u64 frameIndex;

                ReadResult() = default;
                ReadResult(const ReadResult&) = delete;
                ReadResult& operator=(const ReadResult&) = delete;
                ReadResult(ReadResult&& other) = default;
                ReadResult& operator=(ReadResult&& other) = default;

                static ep_sp<ReadResult> Make(ep_u64 size, ep_u64 frameIndex) {
                    auto* result = new ReadResult();
                    result->size = size;
                    result->frameIndex = frameIndex;
                    return ep_sp<ReadResult>(result);
                }

                ep_sp<BufferInfo> pbo;

                struct UsingGuard {
                    ep_sp<BufferInfo::UsingGuard> pboGuard;
                    ep_sp<BufferInfo::UsingGuard::MappingGuard> mapGuard;

                    UsingGuard(const ep_sp<BufferInfo>& pbo)
                        : pboGuard(pbo->useSp())
                        , mapGuard(pboGuard->mapSp(GL_READ_ONLY))
                    {}
                    
                    UsingGuard(const UsingGuard&) = delete;
                    UsingGuard& operator=(const UsingGuard&) = delete;
                    UsingGuard(UsingGuard&& other) = default;
                    UsingGuard& operator=(UsingGuard&& other) = default;

                    ep_u8* data() const {
                        return (ep_u8*)mapGuard->data;
                    }
                };

                UsingGuard use() {
                    return UsingGuard(pbo);
                }
            };

            void requestRead() {
                /* !docs
                Requests read this frame.
                It will automatically call `@flush`.
                */

                flush();

                for (auto& slot : bufferSlots) {
                    if (!slot.sync) {
                        readToSlot(slot);
                        return;
                    }
                }

                addBufferSlot();
                readToSlot(bufferSlots.back());
            }

            using CallbackFunc = std::function<void(ReadResult&)>;
            CallbackFunc callback;

            void flush() {
                /* !docs
                Flushes read results.
                */

                for (auto& slot : bufferSlots) {
                    if (!slot.sync || !slot.sync->isSignaled()) continue;
                    slot.sync = nullptr;

                    auto result = ReadResult::Make(getBufferSize(), slot.frameIndex);
                    result->pbo = std::move(slot.buffer);
                    readResults.push_back(result);
                }

                emit();
            }

            void finish() {
                /* !docs
                Waits for all read results to be flushed.
                */

                glCtx->finish();

                for (auto& slot : bufferSlots) {
                    if (!slot.sync) continue;
                    slot.sync->wait(GL_SYNC_FLUSH_COMMANDS_BIT);
                    flush();
                }
            }

            private:
            struct BufferSlot {
                ep_sp<BufferInfo> buffer;
                ep_sp<TextureInfo> texture;
                ep_sp<SyncInfo> sync;
                ep_u64 frameIndex;
            };

            std::vector<BufferSlot> bufferSlots;
            std::vector<ep_sp<BufferInfo>> pendingBuffers;
            std::vector<ep_sp<ReadResult>> readResults;
            ep_u64 currentFrameIndex;
            ep_u64 lastEmitFrameIndex;

            void addBufferSlot() {
                if (frameWidth % 2 != 0 || frameHeight % 2 != 0) throw std::runtime_error("Frame size must be even");

                auto& slot = bufferSlots.emplace_back();
                slot.buffer = allocPbo();
            }

            void readToSlot(BufferSlot& slot) {
                if (!slot.buffer) slot.buffer = allocPbo();

                if (glCtx->getCurrentIsMultiSampled()) {
                    if (!slot.texture) {
                        slot.texture = glCtx->createTexture();
                        slot.texture->use().image2D(frameWidth, frameHeight, nullptr);
                    }

                    glCtx->copyCurrentToTexture(slot.texture.get());
                    auto fboGuard = slot.texture->frameBuffer->use(slot.texture.get(), GL_FRAMEBUFFER);
                    readToSlotDirect(slot);
                } else {
                    readToSlotDirect(slot);
                }
            }

            void readToSlotDirect(BufferSlot& slot) {
                glCtx->convertToYUV(frameWidth, frameHeight, true);
                auto pboGuard = slot.buffer->use();
                glCtx->gl.glReadPixels(0, 0, frameWidth, getBufferHeight(), GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                slot.sync = glCtx->createSync();
                slot.frameIndex = currentFrameIndex++;
            }

            void emit() {
                while (readResults.size()) {
                    for (ep_u64 i = 0; i < readResults.size(); i++) {
                        auto& result = readResults[i];
                        if (result->frameIndex == 0 || result->frameIndex == lastEmitFrameIndex + 1) {
                            lastEmitFrameIndex = result->frameIndex;
                            callback(*result);
                            pendingBuffers.push_back(std::move(result->pbo));
                            readResults.erase(readResults.begin() + i);
                            break;
                        }
                    }
                }
            }

            ep_sp<BufferInfo> allocPbo() {
                if (pendingBuffers.empty()) {
                    auto pbo = glCtx->createBuffer(GL_PIXEL_PACK_BUFFER);
                    pbo->use().data(getBufferSize(), nullptr, GL_STREAM_READ);
                    return pbo;
                }

                auto ret = std::move(pendingBuffers.back());
                pendingBuffers.pop_back();
                return ret;
            }

            ep_u64 getBufferHeight() noexcept {
                return (frameHeight * 3 + 7) / 8;
            }

            ep_u64 getBufferSize() noexcept {
                return frameWidth * getBufferHeight() * 4;
            }
        };

        AsyncFrameReader createAsyncFrameReader(ep_u64 frameWidth, ep_u64 frameHeight) {
            /* !docs
            Creates an reader for reading frames from the current context.
            */

            AsyncFrameReader reader {};
            reader.glCtx = this;
            reader.frameWidth = frameWidth;
            reader.frameHeight = frameHeight;
            reader.initBufferSlots(4);
            return reader;
        }

        struct FBOGuard {
            GL33Context* glCtx;
            GLint readFbo, drawFbo;

            FBOGuard(GL33Context* glCtx) : glCtx(glCtx) {
                glCtx->gl.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
                glCtx->gl.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
            }

            FBOGuard(const FBOGuard&) = delete;
            FBOGuard& operator=(const FBOGuard&) = delete;
            FBOGuard(FBOGuard&& other) = delete;
            FBOGuard& operator=(FBOGuard&& other) = delete;

            ~FBOGuard() {
                glCtx->gl.glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
                glCtx->gl.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
            }
        };

        FBOGuard getFBOGuard() noexcept {
            /* !docs
            Creates a guard that will restore the draw&read framebuffer bindings when it goes out of scope.
            */

            return FBOGuard(this);
        }

        void convertToYUV(ep_u64 width, ep_u64 height, bool flipY = false) noexcept {
            /* !docs
            Converts the current framebuffer to YUV420 format.
            */

            auto mesh = requestMesh(6);
            mesh.program = preloadedPrograms.yuvConverter.get();
            mesh.color = GLvec4::White();

            auto progGuard = mesh.program->use();
            mesh.program->getUniformLocation("uResolution").seti(width, height);
            mesh.program->getUniformLocation("uFlipY").seti(flipY);
            renderToDrawFbo(width, height, mesh);
        }

        ep_sp<TextureInfo> createTextureFromDecoded(const DecodedRGBATexture& decoded) {
            if (!decoded.valid()) throw std::runtime_error("invalid decoded texture");

            auto tex = createTexture();
            tex->use().image2D(decoded);
            return tex;
        }

        private:
        ep_sp<ProgramInfo> defaultProgram;
        ep_sp<TextureInfo> defaultWhiteTexture;
        ep_sp<VertexPool> vertexPool;
        bool resourcesInitialized = false;
        GLvec4 currentViewport;
        ep_u64 frameSig;

        struct TempTextureSlot {
            ep_sp<TextureInfo> tex;
            bool isUsing;
        };

        std::vector<TempTextureSlot> tempTextureSlots;

        void initDefaultResources() {
            if (resourcesInitialized) return;
            resourcesInitialized = true;
            
            defaultProgram = createConfiguredProgram(defaultFragmentShaderSource);
            preloadedPrograms.gaussianBlur = ProgramPresets::gaussianBlur(this);
            preloadedPrograms.yuvConverter = ProgramPresets::yuvConverter(this);

            unsigned char whiteTextureData[16] = {
                255, 255, 255, 255,
                255, 255, 255, 255,
                255, 255, 255, 255,
                255, 255, 255, 255
            };

            defaultWhiteTexture = createTexture();
            defaultWhiteTexture->use().image2D(
                2, 2,
                (void*)(&whiteTextureData[0])
            );

            enable(GL_BLEND);
            blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            gl.glReadBuffer(GL_COLOR_ATTACHMENT0);
            gl.glDrawBuffer(GL_COLOR_ATTACHMENT0);

            vertexPool = VertexPool::Make();
        }

        struct TempTextureGuard {
            GL33Context* glCtx;
            ep_u64 index;

            TempTextureGuard(GL33Context* glCtx, ep_u64 index, ep_u64 width, ep_u64 height) : glCtx(glCtx), index(index) {
                glCtx->tempTextureSlots[index].isUsing = true;

                auto tex = get();
                if (!tex->sizeIsSame(width, height)) {
                    tex->use().image2D(width, height, nullptr);
                }
            }

            ep_sp<TextureInfo> get() {
                return glCtx->tempTextureSlots[index].tex;
            }

            TempTextureGuard(const TempTextureGuard&) = delete;
            TempTextureGuard& operator=(const TempTextureGuard&) = delete;
            TempTextureGuard(TempTextureGuard&& other) = delete;
            TempTextureGuard& operator=(TempTextureGuard&& other) = delete;

            ~TempTextureGuard() {
                glCtx->tempTextureSlots[index].isUsing = false;
            }
        };

        TempTextureGuard allocTempTexture(ep_u64 width, ep_u64 height) noexcept {
            for (auto& slot : tempTextureSlots) {
                if (!slot.isUsing) return TempTextureGuard(this, &slot - &tempTextureSlots[0], width, height);
            }

            auto& slot = tempTextureSlots.emplace_back();
            slot.tex = createTexture();
            return TempTextureGuard(this, &slot - &tempTextureSlots[0], width, height);
        }
    };

    struct GL33Canvas {
        /* !docs
        A canvas to draw on.
        */

        Transform2D transform;
        GL33Context* glCtx;

        static GL33Canvas Make(GL33Context* glCtx) {
            GL33Canvas canvas {};
            canvas.glCtx  = glCtx;
            return canvas;
        }

        GLvec2 toNDC(const GLvec2& pos) noexcept {
            auto vp = glCtx->getViewport();
            return {
                (pos.x - vp.x) / vp.z * 2.0 - 1.0,
                -((pos.y - vp.y) / vp.w * 2.0 - 1.0)
            };
        }

        GLvec2 transformPoint(const GLvec2& pos) noexcept {
            auto p = transform.transformPoint(pos.x, pos.y);
            return { p.x, p.y };
        }

        void normVertices(VertexPool::AllocResult& vertices) noexcept {
            for (auto& v : vertices) {
                v.position = toNDC(transformPoint(v.position));
            }
        }

        void resetTransform() noexcept { transform = Transform2D(); }
        void translate(ep_f64 x, ep_f64 y) noexcept { transform.translate(x, y); }
        void translate(const GLvec2& pos) noexcept { transform.translate(pos.x, pos.y); }
        void scale(ep_f64 x, ep_f64 y) noexcept { transform.scale(x, y); }
        void scale(const GLvec2& scale) noexcept { transform.scale(scale.x, scale.y); }
        void rotate(ep_f64 angle) noexcept { transform.rotate(angle); }
        void rotateDegrees(ep_f64 angle) noexcept { rotate(angle / 180.0 * std::numbers::pi); }

        void save() noexcept { transformHistory.push_back(transform); }
        void restore() noexcept { transform = transformHistory.back(); transformHistory.pop_back(); }

        void drawMesh(Mesh& mesh) noexcept {
            normVertices(mesh.vertices);
            glCtx->drawMesh(mesh);
        }

        struct DrawRectConfig {
            GLvec2 position, size;
            GLvec4 color = { 1.0, 1.0, 1.0, 1.0 };
            GLvec2 uvPosition = { 0.0, 0.0 };
            GLvec2 uvSize = { 1.0, 1.0 };
            TextureInfo* texture;
        };

        void drawRect(const DrawRectConfig& config) noexcept {
            auto mesh = glCtx->requestMesh(6);
            mesh.color = config.color;
            mesh.texture = config.texture;
            mesh.addRect(config.position, config.size, config.uvPosition, config.uvSize);
            drawMesh(mesh);
        }

        private:
        std::vector<Transform2D> transformHistory;
    };

    struct VideoRecorder {
        /* !docs
        A video recorder, which records YUV420 frames to callback.
        */

        VideoRecorder() = default;
        VideoRecorder(const VideoRecorder&) = delete;
        VideoRecorder& operator=(const VideoRecorder&) = delete;
        VideoRecorder(VideoRecorder&&) = delete;
        VideoRecorder& operator=(VideoRecorder&&) = delete;

        using CallbackFunc = std::function<void(ep_u64)>;
        
        struct Config {
            ep_u64 nominalSize = 1920 * 1080 * 16;
            bool callbackIsThreadSafe = false;
            ep_u64 msaaSamples = 4;
        };

        static ep_sp<VideoRecorder> Make(
            const ep_sp<GL33Context>& glCtx,
            ep_u64 width, ep_u64 height,
            const CallbackFunc& callback,
            const Config& config
        ) {
            auto* recorder = new VideoRecorder();

            recorder->glCtx = glCtx;
            recorder->asyncFrameReader = glCtx->createAsyncFrameReader(width, height);
            recorder->asyncFrameReader.callback = [=](GL33Context::AsyncFrameReader::ReadResult& result) {
                recorder->ensureCallbackIsDone();
                auto slotIndex = recorder->allocYUVFrameSlot();
                auto& slot = recorder->yuvFrameSlots[slotIndex];
                slot.frame->fromPtr(result.use().data());

                if (!config.callbackIsThreadSafe) {
                    recorder->callback(slotIndex);
                } else {
                    recorder->callbackThread = std::thread([=]() {
                        recorder->callback(slotIndex);
                    });
                }
            };

            recorder->callback = callback;

            auto surfacesCount = std::clamp<ep_u64>(
                config.nominalSize / (width * height),
                1, 512
            );

            for (ep_u64 i = 0; i < surfacesCount; i++) {
                if (config.msaaSamples > 1) {
                    auto surface = glCtx->createTexture(GL_TEXTURE_2D_MULTISAMPLE);
                    surface->use().image2DMultisample(width, height, config.msaaSamples);
                    recorder->surfaces.push_back(surface);
                } else {
                    auto surface = glCtx->createTexture(GL_TEXTURE_2D);
                    surface->use().image2D(width, height, nullptr);
                    recorder->surfaces.push_back(surface);
                }
            }

            recorder->maxConcurrentYuvSlots = std::clamp<ep_u64>(
                config.nominalSize / (width * height) * 4,
                1, 4096
            );

            return ep_sp<VideoRecorder>(recorder);
        }

        struct FrameUsingGuard {
            VideoRecorder* ref;

            FrameUsingGuard(VideoRecorder& recorder) : ref(&recorder) {
                auto& surface = ref->surfaces[ref->currentSurfaceIndex];
                ref->currentSurfaceIndex = (ref->currentSurfaceIndex + 1) % ref->surfaces.size();
                fboGuard = surface->frameBuffer->useSp(surface.get(), GL_FRAMEBUFFER);
            }

            FrameUsingGuard(const FrameUsingGuard&) = delete;
            FrameUsingGuard& operator=(const FrameUsingGuard&) = delete;
            FrameUsingGuard(FrameUsingGuard&&) = delete;
            FrameUsingGuard& operator=(FrameUsingGuard&&) = delete;

            ~FrameUsingGuard() {
                ref->asyncFrameReader.requestRead();
            }

            private:
            ep_sp<FramebufferInfo::UsingGuard> fboGuard;
        };

        FrameUsingGuard useFrame() noexcept {
            return FrameUsingGuard(*this);
        }

        YUV420Frame* referYUVFrame(ep_u64 index) noexcept {
            std::lock_guard<std::mutex> guard(yuvFrameSlotsMutex);
            return yuvFrameSlots[index].frame.get();
        }

        void returnYUVFrame(ep_u64 index) noexcept {
            std::lock_guard<std::mutex> guard(yuvFrameSlotsMutex);
            yuvFrameSlots[index].isUsing = false;
        }

        void finish() {
            asyncFrameReader.finish();
            ensureCallbackIsDone();
        }

        ~VideoRecorder() {
            finish();
        }

        private:
        ep_sp<GL33Context> glCtx;
        GL33Context::AsyncFrameReader asyncFrameReader;
        CallbackFunc callback;
        ep_u64 maxConcurrentYuvSlots;

        std::vector<ep_sp<TextureInfo>> surfaces;
        ep_u64 currentSurfaceIndex = 0;

        struct YUVFrameSlot {
            ep_sp<YUV420Frame> frame;
            bool isUsing;
        };

        std::vector<YUVFrameSlot> yuvFrameSlots;
        std::mutex yuvFrameSlotsMutex;
        std::thread callbackThread;

        void ensureCallbackIsDone() noexcept {
            if (callbackThread.joinable()) callbackThread.join();
        }

        ep_u64 allocYUVFrameSlot() {
            while (getYUVFrameSlotsInUse() > maxConcurrentYuvSlots) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }

            std::lock_guard<std::mutex> guard(yuvFrameSlotsMutex);

            for (ep_u64 i = 0; i < yuvFrameSlots.size(); i++) {
                auto& slot = yuvFrameSlots[i];
                if (!slot.isUsing) {
                    slot.isUsing = true;
                    return i;
                }
            }

            auto& slot = yuvFrameSlots.emplace_back();
            slot.frame = YUV420Frame::Make(asyncFrameReader.frameWidth, asyncFrameReader.frameHeight);
            slot.isUsing = true;
            return yuvFrameSlots.size() - 1;
        }

        ep_u64 getYUVFrameSlotsInUse() noexcept {
            std::lock_guard<std::mutex> guard(yuvFrameSlotsMutex);
            return std::count_if(yuvFrameSlots.begin(), yuvFrameSlots.end(), [](const auto& slot) {
                return slot.isUsing;
            });
        }
    };

    struct TextManager {
        static constexpr ep_u64 maxCacheSize = 128;

        using Renderer = std::function<DecodedRGBATexture(const std::string&, ep_u64)>;
        Renderer renderer;
        ep_sp<GL33Context> glCtx;

        void check() {
            checkBoolAndThrow(!!renderer, "renderer is not set", "TextManager");
            checkBoolAndThrow(!!glCtx, "glCtx is not set", "TextManager");
        }

        struct GetTextureResult {
            TextureInfo* texture;
            ep_f64 scale;

            GLvec2 size() const noexcept {
                return texture->size() * scale;
            }
        };

        bool getTexture(
            const std::string& text, ep_f64 fontSize,
            GetTextureResult& result
        ) {
            if (fontSize <= 0.0) return false;

            ep_u64 isize = std::ceil(fontSize / 48) * 48;
            result.scale = fontSize / isize;
            auto key = std::make_pair(text, isize);

            {
                auto it = cache.find(key);
                if (it != cache.end()) {
                    result.texture = it->second.get();
                    return true;
                }
            }

            if (cache.size() >= maxCacheSize) {
                static std::mt19937 rng { std::random_device {} () };
                std::uniform_int_distribution<ep_u64> dist { 0, cache.size() - 1 };
                auto it = cache.begin();
                std::advance(it, dist(rng));
                cache.erase(it);
            }

            auto decoded = renderer(text, isize);
            if (!decoded.valid()) throw std::runtime_error("texture is invalid");

            auto tex = glCtx->createTexture();
            tex->use().image2D(decoded);
            cache[key] = tex;

            result.texture = tex.get();
            return true;
        }

        struct DrawTextConfig {
            std::string text;
            ep_f64 fontSize;
            GLvec2 pos, anchor;
            ep_f64 rotation;
            GLvec2 scale;
            GLvec4 color;

            void normScale() noexcept {
                auto wScale = std::min<ep_f64>(std::max(scale.x, scale.y), 16.0);

                if (wScale > 1.0) {
                    scale /= wScale;
                    fontSize *= wScale;
                }
            }
        };

        void drawText(GL33Canvas& cvs, DrawTextConfig& config) {
            config.normScale();

            static GetTextureResult textureResult;
            if (!getTexture(config.text, config.fontSize, textureResult)) return;

            auto size = textureResult.size() * config.scale;

            cvs.save();
            cvs.translate(config.pos);
            cvs.rotateDegrees(config.rotation);
            cvs.drawRect({
                .position = -size * config.anchor,
                .size = size,
                .color = config.color,
                .texture = textureResult.texture
            });
            cvs.restore();
        }

        private:
        std::map<std::pair<std::string, ep_u64>, ep_sp<TextureInfo>> cache;
    };
};

struct DecodedAudio {
    /* !docs
    A class to store decoded audio data.
    The data is pcm 16-bit signed integer and interleaved.
    */

    std::vector<ep_i16> data;
    ep_u64 channels;
    ep_u64 sampleRate;

    static ep_sp<DecodedAudio> Make() {
        auto* audio = new DecodedAudio();
        return ep_sp<DecodedAudio>(audio);
    }

    ep_u64 getSampleCount() const noexcept {
        return data.size() / channels;
    }

    ep_u64 getSampleCount(ep_u64 sampleRate) const noexcept {
        return (ep_f64)getSampleCount() * sampleRate / this->sampleRate;
    }

    ep_i16 sampleAt(ep_f64 index, ep_u64 channel_index, ep_u64 channels) const noexcept {
        index = std::clamp<ep_f64>(index, 0, getSampleCount() - 1);

        if (channels == this->channels) {
            ep_f64 v1 = data[ep_i64(index) * channels + channel_index];
            ep_f64 v2 = data[ep_i64(std::ceil(index)) * channels + channel_index];
            return typed_clamp<ep_i16, ep_f64>(v1 + (v2 - v1) * (index - ep_i64(index)));
        } else {
            ep_f64 sum = 0;
            for (ep_u64 i = 0; i < this->channels; i++) {
                sum += data[ep_i64(index) * this->channels + i];
            }

            return typed_clamp<ep_i16, ep_f64>(sum / this->channels);
        }
    }

    ep_i16 sampleAt(ep_i64 index, ep_u64 channel_index, ep_u64 channels, ep_u64 sampleRate) const noexcept {
        return sampleAt((ep_f64)index / sampleRate * this->sampleRate, channel_index, channels);
    }

    ep_f64 getLengthInSeconds() const noexcept {
        return (ep_f64)getSampleCount() / sampleRate;
    }

    ep_sp<DecodedAudio> copy() const {
        auto audio = DecodedAudio::Make();
        audio->data = data;
        audio->channels = channels;
        audio->sampleRate = sampleRate;
        return audio;
    }

    void overlapIndex(const ep_sp<DecodedAudio>& other, ep_i64 start_index, ep_f64 volume = 1.0) noexcept {
        ep_i64 end_index = start_index + other->getSampleCount(sampleRate);

        if (start_index > (ep_i64)getSampleCount()) return;
        if (end_index < 0) return;

        start_index = std::max<ep_i64>(0, start_index);
        end_index = std::min<ep_i64>(getSampleCount(), end_index);

        for (ep_i64 i = start_index; i < end_index; i++) {
            for (ep_i64 j = 0; j < (ep_i64)channels; j++) {
                ep_i64 k = i * channels + j;
                data[k] = typed_clamp<ep_i16, ep_f64>((ep_f64)data[k] + other->sampleAt(i - start_index, j, channels, sampleRate) * volume);
            }
        }
    }

    void overlapSecond(const ep_sp<DecodedAudio>& other, ep_f64 start_time, ep_f64 volume = 1.0) noexcept {
        overlapIndex(other, (ep_i64)(start_time * sampleRate), volume);
    }

    void applyVolume(ep_f64 volume) noexcept {
        if (volume == 1.0) return;

        for (ep_u64 i = 0; i < data.size(); i++) {
            data[i] = typed_clamp<ep_i16, ep_f64>((ep_f64)data[i] * volume);
        }
    }

    ep_u64 getSampleBytesSize() const noexcept {
        return data.size() * sizeof(ep_i16);
    }

    Data toWav() const {
        ByteWriter<ByteEndian::Little> writer;

        writer.writeBytes("RIFF");
        writer.write<ep_i32>(getSampleBytesSize() + 36);
        writer.writeBytes("WAVEfmt ");
        writer.write<ep_i32>(16);
        writer.write<ep_i16>(1);
        writer.write<ep_i16>(channels);
        writer.write<ep_i32>(sampleRate);
        writer.write<ep_i32>(sampleRate * channels * sizeof(ep_i16));
        writer.write<ep_i16>(channels * sizeof(ep_i16));
        writer.write<ep_i16>(16);
        writer.writeBytes("data");
        writer.write<ep_i32>(getSampleBytesSize());
        for (ep_u64 i = 0; i < data.size(); i++) {
            writer.write<ep_i16>(data[i]);
        }
        return writer.toData();
    }

    void resample(ep_u64 channels, ep_u64 sampleRate) {
        std::vector<ep_i16> data;
        ep_u64 sampleCount = getSampleCount(sampleRate);
        data.resize(sampleCount * channels);

        for (ep_u64 i = 0; i < sampleCount; i++) {
            for (ep_u64 j = 0; j < channels; j++) {
                data[i * channels + j] = sampleAt(i, j, channels, sampleRate);
            }
        }

        this->data = data;
        this->channels = channels;
        this->sampleRate = sampleRate;
    }
};

struct AudioEngine {
    /* !docs
    An audio engine to play audio.
    */

    AudioEngine() = default;
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    AudioEngine(AudioEngine&&) = delete;
    AudioEngine& operator=(AudioEngine&&) = delete;

    void* audioContext;
    std::function<void(void*)> audioContextDestructor;

    ep_u64 channels;
    ep_u64 sampleRate;

    struct Task {
        ep_sp<DecodedAudio> audio;
        ep_i64 offset;
        ep_f64 volume = 1.0;
        bool stopped;

        static ep_sp<Task> Make() {
            auto* task = new Task();
            return ep_sp<Task>(task);
        }
    };

    ep_i64 currentOffset;
    ep_f64 currentOffsetTime;
    std::vector<ep_sp<Task>> tasks;
    ep_f64 volume = 1.0;

    static ep_sp<AudioEngine> Make() {
        auto* eng = new AudioEngine();
        return ep_sp<AudioEngine>(eng);
    }

    ep_sp<Task> createTask(const ep_sp<DecodedAudio>& audio) noexcept {
        auto task = Task::Make();
        task->audio = audio;
        task->offset = currentOffset;

        std::lock_guard<std::mutex> guard(mtx);
        tasks.push_back(task);
        return task;
    }

    ep_f64 getTaskTime(const ep_sp<Task>& task) const noexcept {
        return (ep_f64)(currentOffset - task->offset) / sampleRate + (globalTimer() - currentOffsetTime);
    }

    bool getTaskEnded(const ep_sp<Task>& task) const noexcept {
        return (task->offset + (ep_i64)task->audio->getSampleCount(sampleRate) <= currentOffset) || task->stopped;
    }

    void seekTask(const ep_sp<Task>& task, ep_f64 time) const noexcept {
        task->offset = currentOffset - (ep_i64)(time * sampleRate);
    }

    void callback(ep_i16* buffer, ep_i64 frameCount) noexcept {
        /* !docs
        Fills the buffer with audio data.
        */

        {
            std::lock_guard<std::mutex> guard(mtx);
            tasks.erase(std::remove_if(
                tasks.begin(),
                tasks.end(),
                [&](const auto& task) { return getTaskEnded(task); }
            ), tasks.end());
            tasksCopied = tasks;
        }

        bufferCache.resize(frameCount * channels);
        std::fill(bufferCache.begin(), bufferCache.end(), 0);

        for (const auto& task : tasksCopied) {
            ep_i64 startSample = currentOffset - task->offset;
            ep_i64 endSample = startSample + frameCount;

            if (startSample > (ep_i64)task->audio->getSampleCount(sampleRate)) continue;
            if (endSample <= 0) continue;

            startSample = std::max<ep_i64>(0, startSample);
            endSample = std::min<ep_i64>(task->audio->getSampleCount(sampleRate), endSample);

            for (ep_i64 i = startSample; i < endSample; i++) {
                for (ep_i64 j = 0; j < (ep_i64)channels; j++) {
                    ep_i16 sample = task->audio->sampleAt(i, j, channels, sampleRate);
                    bufferCache[(i - startSample) * channels + j] += (ep_i32)sample * task->volume;
                }
            }
        }

        for (ep_i64 i = 0; i < (ep_i64)(frameCount * channels); i++) {
            buffer[i] = typed_clamp<ep_i16, ep_i32>(bufferCache[i] * volume);
        }

        currentOffset += frameCount;
        currentOffsetTime = globalTimer();
    }

    ~AudioEngine() {
        if (audioContextDestructor) {
            audioContextDestructor(audioContext);
        }
    }

    private:
    std::mutex mtx;
    std::vector<ep_sp<Task>> tasksCopied;
    std::vector<ep_i32> bufferCache;
};

namespace SharedCalculatedObjects {
    struct CalculatedText {
        std::string text;
        Vec2 position, scale, anchor;
        ep_f64 fontSize, rotation;
        Color color;
    };

    struct CalculatedRect {
        Vec2 position, size;
        ep_f64 rotation;
        Color color;
    };

    struct CalculatedPoly {
        Vec2 p1, p2, p3, p4;
        Color color;

        static CalculatedPoly Make(
            const Vec2& point,
            const Vec2& size,
            const Color& color,
            const Transform2D& transform = Transform2D()
        ) noexcept {
            return {
                .p1 = transform.transformPoint(point),
                .p2 = transform.transformPoint(point + Vec2 { size.x, 0.0 }),
                .p3 = transform.transformPoint(point + size),
                .p4 = transform.transformPoint(point + Vec2 { 0.0, size.y }),
                .color = color
            };
        }
    };
};

namespace TakeOvererComponents {
    struct SharedComp {
        using TextureDeocder = std::function<DecodedRGBATexture(const Data&)>;
        TextureDeocder textureDecoder;

        ep_sp<GL::TextureInfo> illustionTexture;

        void check() {
            checkBoolAndThrow(!!textureDecoder, "textureDecoder is not set", "SharedComp");
        }
    };

    struct AudioManager {
        using Decoder = std::function<ep_sp<DecodedAudio>(const Data&)>;;

        Decoder decoder;
        ep_sp<AudioEngine> engine;
        ep_u64 maxSfxPlaying = 16;

        ep_sp<DecodedAudio> bgmAudio;

        void check() {
            checkBoolAndThrow(!!decoder, "decoder is not set", "AudioManager");
            checkBoolAndThrow(!!engine, "engine is not set", "AudioManager");
        }

        ep_sp<DecodedAudio> decodeAndCheck(const Data& data) {
            auto decoded = decoder(data);
            if (!decoded) throw std::runtime_error("failed to decode audio");
            return decoded;
        }

        void load(const Data& data) {
            bgmAudio = decodeAndCheck(data);
        }

        void load(const std::string& path) { load(Data::MakeFromFile(path)); }

        void startBgm() {
            if (!bgmAudio) throw std::runtime_error("bgm is not loaded");
            stopBgm();
            bgmAudioTask = engine->createTask(bgmAudio);
        }

        ep_f64 getBgmTime() const {
            return bgmAudioTask ? engine->getTaskTime(bgmAudioTask) : 0.0;
        }

        bool getBpmIsEnded() {
            return bgmAudioTask && engine->getTaskEnded(bgmAudioTask);
        }

        void stopBgm() {
            if (bgmAudioTask) {
                bgmAudioTask->stopped = true;
                bgmAudioTask.reset();
            }
        }

        void setBgmVolume(ep_f64 vol) {
            bgmVolume = vol;

            if (bgmAudioTask) {
                bgmAudioTask->volume = bgmVolume;
            }
        }

        void setSfxVolume(ep_f64 vol) {
            sfxVolume = vol;
        }

        void seekBgm(ep_f64 t) {
            if (bgmAudioTask) {
                engine->seekTask(bgmAudioTask, t);
            }
        }

        ep_f64 getBgmLength() {
            return bgmAudio ? bgmAudio->getLengthInSeconds() : 0.0;
        }

        void playSfx(const ep_sp<DecodedAudio>& audio) {
            if (!maxSfxPlaying) return;

            while (playingSfxs.size() >= maxSfxPlaying) {
                auto& task = playingSfxs.front();
                task->stopped = true;
                playingSfxs.erase(playingSfxs.begin());
            }

            auto task = engine->createTask(audio);
            task->volume = sfxVolume;
        }

        private:
        ep_sp<AudioEngine::Task> bgmAudioTask;
        ep_f64 bgmVolume = 1.0, sfxVolume = 1.0;
        std::vector<ep_sp<AudioEngine::Task>> playingSfxs;
    };

    struct LoadChartResultInfo {
        bool success = true;
        std::vector<std::string> errors;

        ep_f64 createObjectTook;
        ep_f64 initTook;

        void checkAndThrow() const {
            if (success) return;
            std::string messages = "";
            for (const auto& error : errors) messages += error + "\n";
            throw std::runtime_error("failed to load chart: " + messages);
        }

        ep_f64 totalTook() const {
            return createObjectTook + initTook;
        }
    };

    struct RenderConfigBase {
        std::optional<ep_f64> time;
        bool flushGl = true;
        bool disableHitsound = false;

        ep_f64 getTime(const AudioManager& audioManager) const noexcept {
            return time.value_or(audioManager.getBgmTime());
        }
    };

    struct RenderResultInfoBase {
        ep_f64 calculatedTook;
        ep_f64 glOperationsTook;
    };

    template <typename T>
    bool renderSharedObject(
        const T& obj,
        const ep_sp<GL::GL33Context>& glCtx,
        GL::GL33Canvas& cvs,
        GL::TextManager& textManager
    ) {
        using namespace GL;

        if (std::holds_alternative<SharedCalculatedObjects::CalculatedText>(obj)) {
            auto& text = std::get<SharedCalculatedObjects::CalculatedText>(obj);

            TextManager::DrawTextConfig config {
                .text = text.text,
                .fontSize = text.fontSize,
                .pos = text.position,
                .anchor = text.anchor,
                .rotation = text.rotation,
                .scale = text.scale,
                .color = text.color
            };

            textManager.drawText(cvs, config);
        } else if (std::holds_alternative<SharedCalculatedObjects::CalculatedRect>(obj)) {
            auto& rect = std::get<SharedCalculatedObjects::CalculatedRect>(obj);

            cvs.save();
            cvs.translate(rect.position);
            cvs.rotateDegrees(rect.rotation);
            cvs.drawRect({
                .position = -rect.size / 2,
                .size = rect.size,
                .color = rect.color
            });
            cvs.restore();
        } else if (std::holds_alternative<SharedCalculatedObjects::CalculatedPoly>(obj)) {
            auto& poly = std::get<SharedCalculatedObjects::CalculatedPoly>(obj);

            auto mesh = glCtx->requestMesh(Mesh::getPolygonVerticesCount(4));
            mesh.addPolygon({ poly.p1, poly.p2, poly.p3, poly.p4 }, { {}, {}, {}, {} });
            mesh.color = poly.color;
            cvs.drawMesh(mesh);
        } else return false;

        return true;
    }
};

static const ep_f64 INF_TIME = 99999.0;
static const Vec2 INF_TZ = { -INF_TIME, INF_TIME };
static const ep_f64 INF_EV = 1e9;

enum class EnumPhiEventType : ep_u64 {
    PositionX, PositionY,
    SelfRotation, AxisRotation,
    MultiplyAlpha, AdditiveAlpha,
    Color,
    ScaleX, ScaleY,
    Speed, SpeedCoefficient,
    Text,
    PhiShaderUniform,
    MAX = PhiShaderUniform + 1
};

enum class EnumPhiNoteType {
    Tap, Drag, Flick, Hold
};

enum class EnumPhiLineAttachUI {
    Pause, Bar,
    ComboNumber, Combo, Score,
    Name, Level,
    None
};

bool phiEventTypeIsMultiply(EnumPhiEventType type) noexcept {
    /* !docs
    Checks if the event type is a multiply type.
    If it returns true, it means that if there are there are `v1` and `v2` in the same time, the final value will be `v1 * v2`.
    */

    return (
        type == EnumPhiEventType::MultiplyAlpha ||
        type == EnumPhiEventType::ScaleX ||
        type == EnumPhiEventType::ScaleY ||
        type == EnumPhiEventType::SpeedCoefficient
    );
}

struct PhiNoteTypeHelper {
    /* !docs
    A helper class for converting phigros note type to `@EnumPhiNoteType`.
    */

    static EnumPhiNoteType FromOfficial(ep_u64 n) {
        if (n == 1) return EnumPhiNoteType::Tap;
        if (n == 2) return EnumPhiNoteType::Drag;
        if (n == 3) return EnumPhiNoteType::Hold;
        if (n == 4) return EnumPhiNoteType::Flick;
        return EnumPhiNoteType::Tap;
    }

    static EnumPhiNoteType FromRPE(ep_u64 n) {
        if (n == 1) return EnumPhiNoteType::Tap;
        if (n == 2) return EnumPhiNoteType::Hold;
        if (n == 3) return EnumPhiNoteType::Flick;
        if (n == 4) return EnumPhiNoteType::Drag;
        return EnumPhiNoteType::Tap;
    }

    static EnumPhiNoteType FromPEC(const std::string& s) {
        if (s == "n1") return EnumPhiNoteType::Tap;
        if (s == "n2") return EnumPhiNoteType::Hold;
        if (s == "n3") return EnumPhiNoteType::Flick;
        if (s == "n4") return EnumPhiNoteType::Drag;
        return EnumPhiNoteType::Tap;
    } 
};

struct PhiLineAttachUIHelper {
    /* !docs
    A helper class for converting phigros line attach ui target to `@EnumPhiLineAttachUI`.
    */

    static EnumPhiLineAttachUI FromString(const std::string& s) {
        if (s == "pause") return EnumPhiLineAttachUI::Pause;
        if (s == "bar") return EnumPhiLineAttachUI::Bar;
        if (s == "combo") return EnumPhiLineAttachUI::Combo;
        if (s == "combonumber") return EnumPhiLineAttachUI::ComboNumber;
        if (s == "score") return EnumPhiLineAttachUI::Score;
        if (s == "name") return EnumPhiLineAttachUI::Name;
        if (s == "level") return EnumPhiLineAttachUI::Level;
        return EnumPhiLineAttachUI::None;
    }
};

struct PhiMeta {
    /* !docs
    The meta information of a phigros chart.
    */

    ep_f64 offset;
    std::string title;
    std::string composer;
    std::string artist;
    std::string charter;
    std::string difficulty;

    ep_u64 rpeVersion = 0;

    bool isHoldCoverAtHead;
    bool isZeroLengthHoldHidden;
    bool isHighNoteHidden;
    bool isRegLineAlphaNoteHidden;
    Vec2 lineWidthUnit, lineHeightUnit;

    ep_f64 coverEllipsis = 1e-5; /* !inline-docs|
    The cover ellipsis of notes.
    It means if the line enabled cover and the note's y position is less than this value, the note will be hidden.
    */

    ep_f64 maxViewRatio = (ep_f64)16 / 9; /* !inline-docs|
    The maximum view ratio of the chart.
    If the view ratio is greater than this value, the chart will be rendered by fitting the width.
    */
    
    Vec2 worldOrigin, worldViewport; /* !inline-docs|
    The world origin and viewport of the chart, used to normalize the positions.
    */
};

struct PhiBPMEvent {
    /* !docs
    A bpm event item for the phigros chart.
    */

    ep_f64 time; // !inline-docs| It is a beat value, not a second value.
    ep_f64 bpm;

    static void SortBpmEvents(std::vector<PhiBPMEvent>& events) {
        std::sort(events.begin(), events.end(), [](const auto& a, const auto& b) {
            return a.time < b.time;
        });
    }
};

struct PhiEventLayerIndexs {
    /* !docs
    The layer indexs preset of a phigros chart.
    */

    static constexpr ep_u64 RPE_MAX = 5;
    static constexpr ep_u64 UNIT = 1000000;

    static constexpr ep_u64 NOTE_ATTRS = UNIT * 1;
    static constexpr ep_u64 NOTE_ATTRS_2 = UNIT * 2;
    static constexpr ep_u64 LINE_DEFAULT = UNIT * 3;
    static constexpr ep_u64 SHADER_UNIFORM_DEFAULT = UNIT * 4;
};

struct PhiEvent {
    /* !docs
    A event item for the phigros chart.
    */

    Vec2 timeZone; // !inline-docs| in seconds.
    Vec2 valueZone;
    EnumPhiEventType type;

    ep_f64 (* easingFunc)(void*, ep_f64);
    ep_f64 (* easingIntFunc)(void*, ep_f64);
    void* easingFuncContext;
    Vec2 easingZone = { 0.0, 1.0 }; // !inline-docs| It is like the easing clip in Re:PhiEdit.

    ep_u64 layerIndex;

    ep_f64 cumulativeValueAtStart; // !inline-docs| It is like the `floorPosition` in official chart.

    ep_f64 getProgressAtTime(ep_f64 t) noexcept {
        // if (t < timeZone.x) return 0.0;
        return std::clamp((t - timeZone.x) / (timeZone.y - timeZone.x), 0.0, 1.0);
    }

    ep_f64 valueAtTime(ep_f64 t) noexcept {
        auto p = getProgressAtTime(t);
        
        if (hasValueEasing()) {
            if (easingZone == Vec2 { 0.0, 1.0 }) {
                p = easingFunc(easingFuncContext, p);
            } else if (easingZone.x < easingZone.y) {
                ep_f64 s = easingFunc(easingFuncContext, easingZone.x);
                ep_f64 e = easingFunc(easingFuncContext, easingZone.y);

                if (e != s) {
                    p = (easingZone.y - easingZone.x) * p + easingZone.x;
                    p = (easingFunc(easingFuncContext, p) - s) / (e - s);
                }
            }
        }

        return valueZone.x + p * (valueZone.y - valueZone.x);
    }

    static ep_f64 getDefaultValue(EnumPhiEventType type) noexcept {
        /* !docs
        Get the default value of a phigros event type.
        It means the event value will be set to this value if the there is no event.
        */

        return phiEventTypeIsMultiply(type) ? 1.0 : 0.0;
    }

    ep_f64 getIntegralValue(ep_f64 t) noexcept {
        auto p = getProgressAtTime(t);
        ep_f64 iv = p * p / 2.0;

        if (hasAllEasing()) {
            if (easingZone == Vec2 { 0.0, 1.0 } ) {
                iv = easingIntFunc(easingFuncContext, p);
            } else if (easingZone.x < easingZone.y) {
                ep_f64 is = easingIntFunc(easingFuncContext, easingZone.x);
                ep_f64 tp = (easingZone.y - easingZone.x) * p + easingZone.x;
                ep_f64 tiv = easingIntFunc(easingFuncContext, tp);
                ep_f64 s = easingFunc(easingFuncContext, easingZone.x);
                iv = (tiv - is - s * (tp - easingZone.x)) / (1.0 - s) / (1.0 - (easingZone.y - easingZone.x));
            }
        }

        ep_f64 res = (timeZone.y - timeZone.x) * (valueZone.x * p + (valueZone.y - valueZone.x) * iv);
        if (t > timeZone.y) res += valueZone.y * (t - timeZone.y);
        return res;
    }

    private:
    bool hasValueEasing() const noexcept { return easingFunc != nullptr; }
    bool hasAllEasing() const noexcept { return easingFunc != nullptr && easingIntFunc != nullptr; }
};

struct PhiAnimLayer {
    /* !docs
    A animation layer for the phigros chart.
    It stores all types of events if they are in the same layer.
    */

    std::vector<PhiEvent> events[(ep_u64)EnumPhiEventType::MAX];

    void addEvent(const PhiEvent& e) { events[(ep_u64)e.type].push_back(e); }
    std::vector<PhiEvent>& getEvents(EnumPhiEventType type) noexcept { return events[(ep_u64)type]; }

    void init() {
        /* !docs
        Initialize the animation layer.
        It will sort the events and calculate the cumulative value.
        */

        for (auto& typedEvents : events) {
            std::sort(typedEvents.begin(), typedEvents.end(), [](const auto& a, const auto& b) {
                return a.timeZone.x < b.timeZone.x;
            });
        }

        initSpeedCumul();
    }

    void updateType(ep_u64 type, ep_f64 t) noexcept {
        /* !docs
        Update the event value of a event type at a time.
        */

        auto& typedEvents = getEvents((EnumPhiEventType)type);
        if (typedEvents.empty()) return;

        if (lastUpdatedTimes[type] == t) return;
        if (lastUpdatedTimes[type] > t) currentIndexs[type] = 0;

        while (
            currentIndexs[type] < typedEvents.size() - 1
            && typedEvents[currentIndexs[type] + 1].timeZone.x <= t
        ) currentIndexs[type]++;

        auto& e = typedEvents[currentIndexs[type]];

        if (type == (ep_u64)EnumPhiEventType::Speed) {
            currentValues[type] = e.cumulativeValueAtStart + e.getIntegralValue(t);
        } else {
            currentValues[type] = e.valueAtTime(t);
        }

        currentValueZones[type] = e.valueZone;
        lastUpdatedTimes[type] = t;
    }

    void updateType(EnumPhiEventType type, ep_f64 t) noexcept {
        updateType((ep_u64)type, t);
    }

    void update(ep_f64 t) noexcept {
        /* !docs
        Update all event values at a time.
        */

        for (ep_u64 type = 0; type < (ep_u64)EnumPhiEventType::MAX; type++) {
            updateType(type, t);
        }
    }

    ep_f64 get(EnumPhiEventType type) noexcept {
        /* !docs
        Get the event value of a event type.
        */

        if (events[(ep_u64)type].empty()) return PhiEvent::getDefaultValue(type);
        return currentValues[(ep_u64)type];
    }

    std::optional<ep_f64> getAlwaysValue(EnumPhiEventType type) noexcept {
        /* !docs
        Get a fixed value of a event type if it is exists.
        */

        auto& typedEvents = getEvents(type);
        if (typedEvents.empty()) return PhiEvent::getDefaultValue(type);

        if (type == EnumPhiEventType::Speed) {
            if (typedEvents.size() == 1 && typedEvents[0].valueZone.isZeroZone() && typedEvents[0].timeZone.x <= -INF_TIME / 2) {
                return typedEvents[0].valueZone.x;
            }

            return std::nullopt;
        }

        ep_f64 fixedValue = typedEvents[0].valueZone.x;
        for (auto& e : typedEvents) {
            if (!e.valueZone.isZeroZone() || fixedValue != e.valueZone.x) {
                return std::nullopt;
            }
        }

        return fixedValue;
    }

    std::optional<Vec2> get_zone(EnumPhiEventType type) noexcept {
        /* !docs
        Get the current valueZone of a event type.
        */

        return currentValueZones[(ep_u64)type];
    }

    private:
    ep_f64 lastUpdatedTimes[(ep_u64)EnumPhiEventType::MAX];
    ep_u64 currentIndexs[(ep_u64)EnumPhiEventType::MAX];
    ep_f64 currentValues[(ep_u64)EnumPhiEventType::MAX];
    std::optional<Vec2> currentValueZones[(ep_u64)EnumPhiEventType::MAX];

    void initSpeedCumul() {
        auto& speedEvents = getEvents(EnumPhiEventType::Speed);
        ep_f64 cumulativeValue = 0.0;

        for (ep_u64 i = 0; i < speedEvents.size(); i++) {
            auto& e = speedEvents[i];
            e.cumulativeValueAtStart = cumulativeValue;

            ep_f64 endTime = i < speedEvents.size() - 1 ? speedEvents[i + 1].timeZone.x : e.timeZone.y;
            cumulativeValue += e.getIntegralValue(endTime);
        }
    }
};

struct PhiAnimGroup {
    /* !docs
    A animation group of the phigros chart.
    It stores all animation layers of a object.
    */

    std::unordered_map<ep_u64, ep_u64> layerIndexMap;
    std::vector<PhiAnimLayer> layers;

    void addEvent(const PhiEvent& e) {
        if (!layerIndexMap.contains(e.layerIndex)) {
            layerIndexMap[e.layerIndex] = layers.size();
            layers.push_back({});
        }

        layers[layerIndexMap[e.layerIndex]].addEvent(e);
    }

    void init() {
        /* !docs
        Initialize all animation layers.
        */

        for (auto& layer : layers) {
            layer.init();
        }
    }

    void updateType(EnumPhiEventType type, ep_f64 t) noexcept {
        /* !docs
        Update the event value of a event type at a time.
        */

        for (auto& layer : layers) {
            layer.updateType(type, t);
        }
    }

    void update(ep_f64 t) noexcept {
        /* !docs
        Update all event values at a time.
        */

        for (auto& layer : layers) {
            layer.update(t);
        }
    }

    ep_f64 get_based(EnumPhiEventType type, ep_f64 baseValue) noexcept {
        /* !docs
        Get the event value of a event type and it will be added/multiplied to the base value.
        */

        ep_f64 value = baseValue;

        for (auto& layer : layers) {
            if (phiEventTypeIsMultiply(type)) value *= layer.get(type);
            else value += layer.get(type);
        }

        return value;
    }

    Vec2 get_zone(EnumPhiEventType type) noexcept {
        /* !docs
        Get the event valueZone of a event type.
        **It only supports single layer event types.**
        */

        for (auto& layer : layers) {
            auto z = layer.get_zone(type);
            if (z.has_value()) return z.value();
        }

        return {};
    }

    std::optional<ep_f64> getAlwaysHashValue(EnumPhiEventType type) {
        /* !docs
        Get the hash value of fixed event values if it is exists.
        */

        ep_f64 result = PhiEvent::getDefaultValue(type);

        for (auto& layer : layers) {
            auto v = layer.getAlwaysValue(type);
            if (!v.has_value()) return std::nullopt;
            if (phiEventTypeIsMultiply(type)) result *= v.value();
            else result += v.value();
        }

        return result;
    }
};

struct PhiAnimator {
    /* !docs
    The animator of a phigros chart.
    It stores all animation groups of the chart.
    */

    std::unordered_map<ep_u64, PhiAnimGroup> groups;

    PhiAnimGroup& requestGroup(ep_u64 index) {
        /* !docs
        Request a new animation group of a object.
        */

        return groups.try_emplace(index, PhiAnimGroup {}).first->second;
    }

    template <typename T>
    PhiAnimGroup& requestGroup(T& obj) {
        return requestGroup(obj.indexer.get());
    }

    template <typename T>
    void addEvent(T& obj, const PhiEvent& e) {
        /* !docs
        Add a event to a object.
        */

        requestGroup(obj).addEvent(e);
    }

    void init() {
        /* !docs
        Initialize all animation groups.
        */

        for (auto& [_, group] : groups) {
            group.init();
        }
    }

    ep_f64 get_based(ep_u64 index, ep_f64 t, EnumPhiEventType type, ep_f64 baseValue) noexcept {
        /* !docs
        Get the event value of the type of a object at a time and it will be added/multiplied to the base value.
        */

        auto group_it = groups.find(index);
        if (group_it == groups.end()) return baseValue;

        auto& group = group_it->second;
        group.updateType(type, t);
        return group.get_based(type, baseValue);
    }

    template <typename T>
    ep_f64 get_based(T& obj, ep_f64 t, EnumPhiEventType type, ep_f64 baseValue) noexcept {
        return get_based(obj.indexer.get(), t, type, baseValue);
    }

    ep_f64 get(ep_u64 index, ep_f64 t, EnumPhiEventType type) noexcept {
        /* !docs
        Get the event value of the type of a object at a time.
        */

        return get_based(index, t, type, PhiEvent::getDefaultValue(type));
    }

    template <typename T>
    ep_f64 get(T& obj, ep_f64 t, EnumPhiEventType type) noexcept {
        return get(obj.indexer.get(), t, type);
    }

    Vec2 get_zone(ep_u64 index, ep_f64 t, EnumPhiEventType type) noexcept {
        /* !docs
        Get the event valueZone of the type of a object at a time.
        **It only supports single layer event types.**
        */

        auto group_it = groups.find(index);
        if (group_it == groups.end()) return {};

        auto& group = group_it->second;
        group.updateType(type, t);
        return group.get_zone(type);
    }

    template <typename T>
    Vec2 get_zone(T& obj, ep_f64 t, EnumPhiEventType type) noexcept {
        return get_zone(obj.indexer.get(), t, type);
    }

    // std::nullopt means it is unpredictable
    template <typename T>
    std::optional<ep_u64> get_note_group_hash(T& note) {
        /* !docs
        Get the hash value of the events of a note.
        */

        HashBucket hash;

        auto group_it = groups.find(note.indexer.get());

        for (const auto type : {
            EnumPhiEventType::PositionY,
            EnumPhiEventType::SelfRotation,
            EnumPhiEventType::AxisRotation,
            EnumPhiEventType::ScaleY,
            EnumPhiEventType::Speed,
            EnumPhiEventType::SpeedCoefficient
        }) {
            if (group_it == groups.end()) {
                hash.submitNumber(PhiEvent::getDefaultValue(type));
            } else {
                auto v = group_it->second.getAlwaysHashValue(type);
                if (!v.has_value()) return std::nullopt;
                hash.submitNumber(v.value());
            }
        }

        return hash.getHash();
    }

    ep_f64 get_alpha(ep_u64 index, ep_f64 t, ep_f64 additionalDefault) noexcept {
        /* !docs
        Get the alpha value of a object at a time and `EnumPhiEventType::AdditiveAlpha` value is based on the additionalDefault.
        */

        return get(index, t, EnumPhiEventType::MultiplyAlpha) * get_based(index, t, EnumPhiEventType::AdditiveAlpha, additionalDefault);
    }

    template <typename T>
    ep_f64 get_alpha(T& obj, ep_f64 t, ep_f64 additionalDefault) noexcept {
        return get_alpha(obj.indexer.get(), t, additionalDefault);
    }
};

struct PhiNote {
    /* !docs
    A note of the phigros chart.
    */

    ObjectIndexer indexer;

    struct State {
        ep_f64 lastUpdateTime;
        bool playedHitsound;

        void timeUpdated(ep_f64 t) noexcept {
            if (lastUpdateTime > t) {
                playedHitsound = false;
            }

            lastUpdateTime = t;
        }

        bool onPlayHitsound() noexcept {
            /* !docs
            Check if the note should play hitsound.
            */

            if (!playedHitsound) {
                playedHitsound = true;
                return true;
            }

            return false;
        }
    };

    EnumPhiNoteType type;
    ep_f64 time, holdTime;
    bool isFake;

    ep_u64 lineIndex;
    Vec2 floorPosition;
    std::optional<ep_f64> fixedHoldSpeed; // !inline-docs| The speed of the hold note when it is clicked.
    bool isSimul;
    bool isReversedCover; // !inline-docs| Whether the note is reversed cover, used to below a note.

    State state;

    void init(PhiAnimator& animator) {
        floorPosition = { getFloorPositionAt(time, animator), getFloorPositionAt(time + holdTime, animator) };
    }

    ep_f64 getFloorPositionAt(ep_f64 t, PhiAnimator& animator) noexcept {
        /* !docs
        Get the floor position of the note at a time.
        */

        if (t > time && fixedHoldSpeed.has_value()) {
            return getFloorPositionAt(time, animator) + (t - time) * fixedHoldSpeed.value();
        }

        return animator.get(lineIndex, t, EnumPhiEventType::Speed) + animator.get(*this, t, EnumPhiEventType::Speed);
    }

    bool isHold() noexcept {
        return holdTime > 0.0 || type == EnumPhiNoteType::Hold;
    }

    void reverseCover() {
        isReversedCover = !isReversedCover;
    }
};

struct PhiNoteGroup {
    /* !docs
    A group of the phigros chart, used to do optimization.
    */

    struct State {
        ep_f64 lastUpdateTime;
        ep_u64 firstNoteIndex;

        void timeUpdated(ep_f64 t) noexcept {
            if (lastUpdateTime > t) {
                firstNoteIndex = 0;
            }

            lastUpdateTime = t;
        }

        void passedNoteIndex(ep_u64 index) noexcept {
            if (firstNoteIndex == index) {
                firstNoteIndex++;
            }
        }
    };
    
    std::vector<ep_u64> indexs; // !inline-docs| The indexs of the notes in their line.
    bool breakable = true;

    State state;
};

struct PhiLine {
    /* !docs
    A line of the phigros chart.
    */

    ObjectIndexer indexer;

    std::vector<PhiBPMEvent> bpms;
    std::vector<PhiNote> notes;

    std::optional<ep_u64> fatherLineIndex; // !inline-docs| The index of its father line.
    ep_f64 zOrder;
    bool enableCover;
    Vec2 anchor = { 0.5, 0.5 }; // !inline-docs| The anchor of the line's texture.

    std::optional<std::string> textureName; // !inline-docs| The name of the texture, used to load the texture.
    std::optional<EnumPhiLineAttachUI> attachUI;

    std::vector<PhiNoteGroup> noteGroups;

    void init(PhiAnimator& animator) {
        std::stable_sort(notes.begin(), notes.end(), [](const auto& a, const auto& b) {
            return a.time < b.time;
        });

        noteGroups.emplace_back().breakable = false;
        std::unordered_map<ep_u64, ep_u64> noteGroupMap;

        for (ep_u64 i = 0; i < notes.size(); i++) {
            auto& note = notes[i];
            note.lineIndex = indexer.get();
            note.init(animator);

            auto hash = animator.get_note_group_hash(note);
            if (hash.has_value()) {
                if (!noteGroupMap.contains(hash.value())) {
                    noteGroups.emplace_back();
                    noteGroupMap[hash.value()] = noteGroups.size() - 1;
                }

                auto& group = noteGroups[noteGroupMap[hash.value()]];
                group.indexs.push_back(i);
            } else noteGroups[0].indexs.push_back(i);
        }
    }

    ep_f64 beat2sec(ep_f64 beat) const {
        if (bpms.size() == 1) return beat * (60.0 / bpms[0].bpm);

        ep_f64 t = 0.0;

        for (ep_u64 i = 0; i < bpms.size(); i++) {
            auto& e = bpms[i];
            ep_f64 spb = 60.0 / e.bpm;

            if (i != bpms.size() - 1) {
                ep_f64 et_beat = bpms[i + 1].time - e.time;

                if (beat >= et_beat) {
                    t += et_beat * spb;
                    beat -= et_beat;
                } else {
                    t += beat * spb;
                    break;
                }
            } else {
                t += beat * spb;
            }
        }

        return t;
    }

    ep_f64 sec2beat(ep_f64 t) const {
        if (bpms.size() == 1) return t / (60.0 / bpms[0].bpm);

        ep_f64 beat = 0.0;

        for (ep_u64 i = 0; i < bpms.size(); i++) {
            auto& e = bpms[i];
            ep_f64 spb = 60.0 / e.bpm;

            if (i != bpms.size() - 1) {
                ep_f64 et_beat = bpms[i + 1].time - e.time;
                ep_f64 et_sec = et_beat * spb;

                if (t >= et_sec) {
                    beat += et_beat;
                    t -= et_sec;
                } else {
                    beat += t / spb;
                    break;
                }
            } else {
                beat += t / spb;
            }
        }

        return beat;
    }

    ep_f64 getBpmAtSecond(ep_f64 t) const noexcept {
        /* !docs
        Get the bpm at the given time.
        */

        if (bpms.size() == 1) return bpms[0].bpm;

        for (ep_u64 i = 0; i < bpms.size(); i++) {
            auto& e = bpms[i];

            if (i != bpms.size() - 1) {
                ep_f64 et_beat = bpms[i + 1].time - e.time;
                ep_f64 et_sec = et_beat * (60.0 / e.bpm);

                if (t >= et_sec) {
                    t -= et_sec;
                } else {
                    return e.bpm;
                }
            } else {
                return e.bpm;
            }
        }

        return 120.0;
    }
};

struct PhiExtraEffectItem {
    /* !docs
    A extra effect item of the phigros chart.
    */

    Vec2 timeZone;
    std::optional<ep_u64> targetLine;
    ep_u64 order;
    bool isGlobal;
    std::string shaderName;
    ep_u64 shaderId; // !inline-docs| The id of the shader, used to index the shader faster for renderer.
    std::unordered_map<std::string, PhiAnimLayer> uniforms;
};

struct PhiExtra {
    /* !docs
    The extra of a phigros chart.
    */

    std::vector<PhiExtraEffectItem> effects;
    std::vector<ep_u64> zOrderSortedEffects;

    void init() {
        initZOrderSortedEffects();
    }

    private:
    void initZOrderSortedEffects() {
        zOrderSortedEffects.clear();
        for (ep_u64 i = 0; i < effects.size(); i++) zOrderSortedEffects.push_back(i);

        std::stable_sort(zOrderSortedEffects.begin(), zOrderSortedEffects.end(), [&](ep_u64 a, ep_u64 b){
            return effects[a].order < effects[b].order;
        });
    }
};

struct PhiShaderUniform {
    /* !docs
    A shader uniform of the phigros chart.
    */

    ep_u8 used;
    ep_f64 value[4];

    PhiShaderUniform(ep_f64 v0, ep_f64 v1, ep_f64 v2, ep_f64 v3) : used(4), value{ v0, v1, v2, v3 } {}
    PhiShaderUniform(ep_f64 v0, ep_f64 v1, ep_f64 v2) : used(3), value{ v0, v1, v2, 0.0 } {}
    PhiShaderUniform(ep_f64 v0, ep_f64 v1) : used(2), value{ v0, v1, 0.0, 0.0 } {}
    PhiShaderUniform(ep_f64 v0) : used(1), value{ v0, 0.0, 0.0, 0.0 } {}
    PhiShaderUniform() : used(0) {}

    PhiShaderUniform(const std::vector<ep_f64>& v) : used(v.size()), value{} {
        for (ep_u8 i = 0; i < v.size(); i++) value[i] = v[i];
    }

    static PhiShaderUniform Interpolate(const PhiShaderUniform& a, const PhiShaderUniform& b, ep_f64 t) noexcept {
        PhiShaderUniform result;
        result.used = std::max(a.used, b.used);
        for (ep_u8 i = 0; i < 4; i++) result.value[i] = a.value[i] + (b.value[i] - a.value[i]) * t;
        return result;
    }

    bool operator==(const PhiShaderUniform& other) const {
        if (used != other.used) return false;
        for (ep_u8 i = 0; i < 4; i++) if (value[i] != other.value[i]) return false;
        return true;
    }

    bool operator!=(const PhiShaderUniform& other) const { return !(*this == other); }

    void setToGlLocation(GL::ProgramInfo::Location loc) const noexcept {
        if (used == 1) loc.setf(value[0]);
        else if (used == 2) loc.setf(value[0], value[1]);
        else if (used == 3) loc.setf(value[0], value[1], value[2]);
        else if (used == 4) loc.setf(value[0], value[1], value[2], value[3]);
    }
};

struct PhiStoryboardAssets {
    /* !docs
    The assets of the storyboard of a phigros chart.
    */

    // 用于区分是否到达了第一个
    static constexpr ep_f64 kTextIndexOffset = 1;
    static constexpr ep_f64 kColorIndexOffset = 1;
    static constexpr ep_f64 kShaderUniformIndexOffset = 1;

    std::vector<std::string> texts;
    std::unordered_map<std::string, std::pair<ep_u64, Vec2>> textures; // name, (id, size)
    std::vector<Color> colors;
    std::vector<PhiShaderUniform> shaderUniforms;

    std::unordered_map<ep_u64, std::string> shaderNameMap;

    std::function<std::optional<std::pair<ep_u64, Vec2>>(std::string)> textureLoader;
    std::function<void(ep_u64)> textureDestroyer;
    std::function<void(std::string, ep_u64)> shaderPreloader;

    Vec2 requestTextPair(const std::string& start, const std::string& end) {
        Vec2 valueZone;
        if (texts.empty() || texts[texts.size() - 1] != start) texts.push_back(start);
        valueZone.x = texts.size() - 1;
        if (texts.empty() || texts[texts.size() - 1] != end) texts.push_back(end);
        valueZone.y = texts.size() - 1;
        return valueZone + kTextIndexOffset;
    }

    Vec2 requestColorPair(const Color& start, const Color& end) {
        Vec2 valueZone;
        if (colors.empty() || colors[colors.size() - 1] != start) colors.push_back(start);
        valueZone.x = colors.size() - 1;
        if (colors.empty() || colors[colors.size() - 1] != end) colors.push_back(end);
        valueZone.y = colors.size() - 1;
        return valueZone + kColorIndexOffset;
    }

    Vec2 requestShaderUniformPair(const PhiShaderUniform& start, const PhiShaderUniform& end) {
        Vec2 valueZone;
        if (shaderUniforms.empty() || shaderUniforms[shaderUniforms.size() - 1] != start) shaderUniforms.push_back(start);
        valueZone.x = shaderUniforms.size() - 1;
        if (shaderUniforms.empty() || shaderUniforms[shaderUniforms.size() - 1] != end) shaderUniforms.push_back(end);
        valueZone.y = shaderUniforms.size() - 1;
        return valueZone + kShaderUniformIndexOffset;
    }

    Vec2 requestShaderUniformPair(ep_f64 start, ep_f64 end) {
        return requestShaderUniformPair(PhiShaderUniform(start), PhiShaderUniform(end));
    }

    static std::string textInterplate(const std::string& s, const std::string& e, ep_f64 p) {
        auto sps = s.find("%P%");
        auto eps = e.find("%P%");
        
        if (sps != std::string::npos && eps != std::string::npos) {
            ep_f64 sv = 0.0, ev = 0.0;
            try { sv = std::stod(replaceStringWith(s, "%P%", "")); } catch (...) {}
            try { ev = std::stod(replaceStringWith(e, "%P%", "")); } catch (...) {}
            auto v = (ev - sv) * p + sv;

            if (std::fmod(sv, 1.0) == 0.0 && std::fmod(ev, 1.0) == 0.0) {
                return formatToStdString("%.0f", v);
            } else {
                return formatToStdString("%.3f", v);
            }
        } else if (s.empty() && e.empty()) return "";
        else if (e.empty()) return textInterplate(e, replaceStringWith(s, "%P%", ""), 1.0 - p);
        else if (s.empty()) return stringSliceProgress(e, p);
        else {
            ep_i64 ml = std::min(s.size(), e.size());
            if (s.substr(0, ml) == e.substr(0, ml)) {
                auto take = (ep_i64)std::round((ep_f64)((e.size() - s.size()) * p)) + s.size();
                return s + stringSliceProgress(e.substr(ml, e.size() - ml), p);
            } else return replaceStringWith(s, "%P%", "");
        }
    }

    std::optional<std::string> getText(ep_f64 index, const Vec2& valueZone) noexcept {
        if (valueZone.x < kTextIndexOffset) return std::nullopt;

        auto start = texts[(ep_u64)valueZone.x - kTextIndexOffset];
        auto end = texts[(ep_u64)valueZone.y - kTextIndexOffset];
        auto p = index - valueZone.x;

        // TODO: text interpolation
        return textInterplate(start, end, p);
    }

    Color getColor(ep_f64 index, const Color& defaultValue, const Vec2& valueZone) noexcept {
        if (valueZone.x < kColorIndexOffset) return defaultValue;

        auto start = colors[(ep_u64)valueZone.x - kColorIndexOffset];
        auto end = colors[(ep_u64)valueZone.y - kColorIndexOffset];
        auto p = index - valueZone.x;
        return start * (1.0 - p) + end * p;
    }

    PhiShaderUniform getShaderUniform(ep_f64 index, const PhiShaderUniform& defaultValue, const Vec2& valueZone) noexcept {
        if (valueZone.x < kShaderUniformIndexOffset) return defaultValue;

        auto start = shaderUniforms[(ep_u64)valueZone.x - kShaderUniformIndexOffset];
        auto end = shaderUniforms[(ep_u64)valueZone.y - kShaderUniformIndexOffset];
        auto p = index - valueZone.x;
        return PhiShaderUniform::Interpolate(start, end, p);
    }

    bool requestLoadTexture(const std::string& name) {
        if (textures.contains(name)) return true;
        if (!textureLoader) return false;

        auto id = textureLoader(name);

        if (id.has_value()) {
            textures[name] = id.value();
            return true;
        }

        return false;
    }

    bool isTextureLoaded(const std::string& name) noexcept {
        return textures.contains(name);
    }

    std::pair<ep_u64, Vec2>& getTexture(const std::string& name) noexcept {
        return textures[name];
    }

    void clearTextures() {
        for (auto& [_, texture] : textures) {
            if (textureDestroyer) {
                textureDestroyer(texture.first);
            }
        }

        textures.clear();
    }

    ep_u64 requestShaderName(const std::string& name) {
        ep_u64 id = shaderNameMap.size();
        shaderNameMap[id] = name;
        return id;
    }

    std::string getShaderName(ep_u64 id) noexcept {
        return shaderNameMap[id];
    }

    ~PhiStoryboardAssets() {
        clearTextures();
    }
};

struct PhiHitEffectItem {
    /* !docs
    A hit effect item of the phigros chart.
    */

    struct Particle {
        ep_f64 dt, rotation, size;
    };

    ep_f64 time;
    ep_u64 lineIndex, noteIndex;
    std::vector<Particle> particles;
};

struct PhiChart {
    /* !docs
    The phigros chart.
    */

    struct State {
        ep_f64 lastUpdateTime;
        ep_u64 firstHitEffectIndex;

        void timeUpdated(ep_f64 t) noexcept {
            if (lastUpdateTime > t) {
                firstHitEffectIndex = 0;
            }

            lastUpdateTime = t;
        }

        void passedHitEffectIndex(ep_u64 index) noexcept {
            if (firstHitEffectIndex == index) {
                firstHitEffectIndex++;
            }
        }
    };

    struct UserOptions {
        ep_f64 noteScaling = 1.0;

        ep_f64 unsafeBackgroundDim = 0.8;
        ep_f64 backgroundDim = 0.6;
        ep_f64 backgroundTextureBlurRadius = (ep_f64)1 / 20;

        Color lineDefaultColor = { (ep_f64)0xff / 0xff, (ep_f64)0xec / 0xff, (ep_f64)0x9f / 0xff, 1.0 };

        /* !docs
        The color of the progress bar.
        `.first` is the body color, `.second` is the head color.
        */
        std::pair<Color, Color> progressBarDefaultColor = {
            { (ep_f64)145 / 255, (ep_f64)145 / 255, (ep_f64)145 / 255, 0.85 },
            { 1.0, 1.0, 1.0, 0.9 }
        };

        Vec2 storyboardTextBaseSize = { 0.028125, 0.0 };

        enum class EnumStoryboardTextureSclaingBehavior {
            AboutWidth,
            AboutHeight,
            Stretch
        };
        EnumStoryboardTextureSclaingBehavior storyboardTextureSclaingBehavior = EnumStoryboardTextureSclaingBehavior::AboutWidth; // !inline-docs| The behavior of the storyboard texture scaling.
        Vec2 storyboardTextureScaling = { 1.0, 1.0 };

        ep_f64 hitEffectDuration = 0.5;
        ep_f64 hitEffectAlpha = (ep_f64)0xe1 / 0xff;
        ep_f64 hitEffectTextureScaling = 1.54;
        ep_f64 hitEffectParticleSize = 1.0;
        ep_f64 hitEffectParticleDistance = 1.0;

        bool enableNoteOffScreenBreakOptimization = true;
    };

    PhiMeta meta;
    std::vector<PhiLine> lines;
    PhiAnimator animator;
    PhiStoryboardAssets storyboardAssets;
    PhiExtra extra;

    std::vector<PhiHitEffectItem> hitEffects;
    std::vector<ep_f64> comboTimes;
    std::vector<ep_u64> zOrderSortedLines;
    ep_u64 rawHash;

    UserOptions options;

    State state;

    void init() {
        animator.init();

        for (auto& line : lines) {
            if (line.textureName.has_value()) {
                storyboardAssets.requestLoadTexture(line.textureName.value());
            }

            line.init(animator);
        }

        std::unordered_set<std::string> shaderNames;
        for (auto& effect : extra.effects) {
            shaderNames.insert(effect.shaderName);

            for (auto& [_, layer] : effect.uniforms) {
                layer.init();
            }
        }
        
        if (storyboardAssets.shaderPreloader) {
            std::unordered_map<std::string, ep_u64> shaderNameMapInv;

            for (auto& name : shaderNames) {
                auto id = storyboardAssets.requestShaderName(name);
                storyboardAssets.shaderPreloader(name, id);
                shaderNameMapInv[name] = id;
            }

            for (auto& effect : extra.effects) {
                effect.shaderId = shaderNameMapInv[effect.shaderName];
            }
        }

        extra.init();
        
        initSimulNote();
        initHitEffects();
        initPlayemntInfo();
        initZOrderSortedLines();
    }

    Vec2 getLinePositionRaw(ep_f64 t, PhiLine& line) noexcept {
        /* !docs
        Get the position of a line at a time.
        The result is not scaled and not processed father line.
        */

        return {
            animator.get(line, t, EnumPhiEventType::PositionX),
            animator.get(line, t, EnumPhiEventType::PositionY)
        };
    }

    Vec2 getLinePositionRelOrigin(ep_f64 t, PhiLine& line, const Vec2& screenSize) noexcept {
        /* !docs
        Get the position of a line at a time.
        The result is not scaled but processed father line and it is origin relative.
        */

        Vec2 pos = getLinePositionRaw(t, line);
        pos = pos / meta.worldViewport * screenSize;

        if (line.fatherLineIndex.has_value()) {
            auto fatherLineIndex = line.fatherLineIndex.value();
            if (0 <= fatherLineIndex && fatherLineIndex < lines.size()) {
                auto& fatherLine = lines[fatherLineIndex];
                auto fatherLinePosition = getLinePositionRelOrigin(t, fatherLine, screenSize);
                auto fatherLineRotation = animator.get(fatherLine, t, EnumPhiEventType::SelfRotation);

                pos = fatherLinePosition.rotateDegrees(
                    fatherLineRotation + std::atan2(pos.y, pos.x) * 180.0 / std::numbers::pi,
                    pos.length()
                );
            }
        }

        return pos;
    }

    Vec2 getLinePosition(ep_f64 t, PhiLine& line, const Vec2& screenSize) noexcept {
        /* !docs
        Get the position of a line at a time.
        The result is scaled and processed father line.
        */


        Vec2 ori = getLinePositionRelOrigin(t, line, screenSize);
        return ori - meta.worldOrigin / meta.worldViewport * screenSize;
    }

    struct NoteFrameInfo {
        /* !docs
        Information of a note at a time.
        */

        Vec2 headPosition, tailPosition;
        bool isArrived = false;
        bool isVisible = true;
        ep_f64 lineRotation, textureRotation, speedVectorRotation;
        Color color;
        Vec2 scale;
        ep_f64 speedCoefficient;
    };

    NoteFrameInfo getNoteFrameInfo(
        PhiLine& line, PhiNote& note,
        ep_f64 time, const Vec2& screenSize
    ) noexcept {
        /* !docs
        Get the information of a note at a time.
        */

        NoteFrameInfo info {};

        auto linePosition = getLinePosition(time, line, screenSize);
        auto lineRotation = animator.get(line, time, EnumPhiEventType::SelfRotation);
        auto lineSpeedCoefficient = animator.get(line, time, EnumPhiEventType::SpeedCoefficient);
        auto lineAlpha = animator.get_alpha(line, time, 0.0);
        auto noteRotation = animator.get(note, time, EnumPhiEventType::SelfRotation);
        auto noteAxisRotation = animator.get(note, time, EnumPhiEventType::AxisRotation);
        auto noteColorIndex = animator.get(note, time, EnumPhiEventType::Color);
        auto noteColorIndexZone = animator.get_zone(note, time, EnumPhiEventType::Color);
        auto noteColor = storyboardAssets.getColor(noteColorIndex, { 1.0, 1.0, 1.0, 1.0 }, noteColorIndexZone);
        auto noteAlpha = animator.get_alpha(note, time, 1.0);
        auto noteScaling = Vec2 {
            animator.get(note, time, EnumPhiEventType::ScaleX),
            animator.get(note, time, EnumPhiEventType::ScaleY)
        };
        
        Transform2D lineTransform {};
        lineTransform.translate(linePosition);
        lineTransform.rotateDegrees(lineRotation);
        lineTransform.rotateDegrees(noteAxisRotation);
        lineTransform.scale(screenSize);
        lineTransform.scale(1.0, -1.0);

        info.isArrived = time >= note.time;
        auto noteTotalFloorPosition = note.getFloorPositionAt(time, animator);
        auto noteSpeedCoefficient = lineSpeedCoefficient * animator.get(note, time, EnumPhiEventType::SpeedCoefficient);
        auto noteFloorPosition = (note.floorPosition - noteTotalFloorPosition) * noteSpeedCoefficient;
        Vec2 noteBasePosition = { animator.get(note, time, EnumPhiEventType::PositionX), animator.get(note, time, EnumPhiEventType::PositionY) };

        auto noteRelPositionHead = noteBasePosition + Vec2 { 0.0, info.isArrived ? 0.0 : noteFloorPosition.x },
             noteRelPositionTail = noteBasePosition + Vec2 { 0.0, noteFloorPosition.y };
        
        if (line.enableCover && !info.isArrived) {
            if (meta.isHoldCoverAtHead && noteRelPositionHead.y * (note.isReversedCover ? -1.0 : 1.0) < -meta.coverEllipsis) info.isVisible = false;
            if (!meta.isHoldCoverAtHead && noteRelPositionTail.y * (note.isReversedCover ? -1.0 : 1.0) < -meta.coverEllipsis) info.isVisible = false;
        }

        if (note.isHold() && meta.isZeroLengthHoldHidden && note.floorPosition.xyDiff() == 0) info.isVisible = false;
        if (noteRelPositionHead.y > 2.0 && meta.isHighNoteHidden) info.isVisible = false;
        if (meta.isRegLineAlphaNoteHidden && lineAlpha < 0.0) info.isVisible = false;

        info.headPosition = lineTransform.transformPoint(noteRelPositionHead);
        info.tailPosition = lineTransform.transformPoint(noteRelPositionTail);
        info.lineRotation = lineRotation;
        info.textureRotation = lineRotation + noteRotation + noteAxisRotation;
        info.speedVectorRotation = lineRotation + noteAxisRotation;
        if (noteSpeedCoefficient < 0) info.speedVectorRotation += 180.0;
        info.color = noteColor.applyAlpha(noteAlpha);
        info.scale = noteScaling;
        info.speedCoefficient = noteSpeedCoefficient;

        return info;
    }

    ep_u64 getCombo(ep_f64 t) const noexcept {
        /* !docs
        Get the combo at a time.
        */

        if (comboTimes.empty() || comboTimes[0] > t) return 0;

        ep_u64 left = 0, right = comboTimes.size() - 1;
        ep_u64 ans = 1;

        while (left <= right) {
            ep_u64 mid = left + (right - left) / 2;
            if (comboTimes[mid] <= t) {
                ans = mid + 1;
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }

        return ans;
    }

    private:
    void initSimulNote() {
        std::unordered_map<ep_f64, ep_u64> noteTimes;
        for (auto& line : lines) {
            for (auto& note : line.notes) {
                noteTimes[note.time]++;
            }
        }

        for (auto& line : lines) {
            for (auto& note : line.notes) {
                note.isSimul = noteTimes[note.time] > 1;
            }
        }
    }

    void initHitEffects() {
        std::mt19937 rng { std::random_device {} () };
        std::uniform_real_distribution<ep_f64> rng_dist { 0.0, 1.0 };
        auto uniform = [&](ep_f64 a, ep_f64 b) { return a + (b - a) * rng_dist(rng); };

        hitEffects.clear();

        for (auto& line : lines) {
            for (auto& note : line.notes) {
                if (note.isFake) continue;

                ep_f64 t = note.time;
                while (t <= note.time + note.holdTime) {
                    auto& item = hitEffects.emplace_back();
                    item.time = t;
                    item.lineIndex = &line - lines.data();
                    item.noteIndex = &note - line.notes.data();

                    for (ep_u64 i = 0; i < 4; i++) {
                        auto& particle = item.particles.emplace_back();
                        particle.rotation = uniform(0.0, 360.0);
                        particle.size = uniform(185.0, 265.0);
                    }

                    t += 30.0 / line.getBpmAtSecond(t);
                }
            }
        }

        std::stable_sort(hitEffects.begin(), hitEffects.end(), [](const auto& a, const auto& b) {
            return a.time < b.time;
        });
    }

    void initPlayemntInfo() {
        for (auto& line : lines) {
            for (auto& note : line.notes) {
                if (note.isFake) continue;
                comboTimes.push_back(note.time + std::max(0.0, note.holdTime - 0.2));
            }
        }

        std::sort(comboTimes.begin(), comboTimes.end());
    }

    void initZOrderSortedLines() {
        zOrderSortedLines.clear();
        for (ep_u64 i = 0; i < lines.size(); i++) zOrderSortedLines.push_back(i);

        std::stable_sort(zOrderSortedLines.begin(), zOrderSortedLines.end(), [&](ep_u64 a, ep_u64 b){
            return lines[a].zOrder < lines[b].zOrder;
        });
    }
};

struct PhiChartLoadResult {
    /* !docs
    The result of loading a phigros chart.
    */

    bool success;
    std::vector<std::string> errors;
    PhiChart chart;
};

#define CHART_LOAD_FAILED(prefix, err) \
    { \
        return PhiChartLoadResult { \
            .success = false, \
            .errors = { std::string(prefix) + ": " + (err) } \
        }; \
    }

PhiChartLoadResult loadPhiChartFromOfficialJson(const Data& data) {
    /* !docs
    Loads a phigros chart from an official json data.
    */

    JsonNode jsonRoot;
    auto [jsonParseSuccess, err] = JsonNode::Parse(&jsonRoot, data);
    if (!jsonParseSuccess) CHART_LOAD_FAILED("official", std::string("failed to parse json: ") + err);

    PhiChart chart {};

    if (!jsonRoot.isObject()) CHART_LOAD_FAILED("official", "root is not an object");

    if (!jsonRoot.hasKey("formatVersion")) CHART_LOAD_FAILED("official", "missing formatVersion field");
    if (!jsonRoot["formatVersion"].isNumber()) CHART_LOAD_FAILED("official", "formatVersion is not a number");
    ep_u64 formatVersion = jsonRoot["formatVersion"].getNumber();

    chart.meta.isHoldCoverAtHead = true;
    chart.meta.isZeroLengthHoldHidden = true;
    chart.meta.isHighNoteHidden = true;
    chart.meta.isRegLineAlphaNoteHidden = false;
    chart.meta.lineWidthUnit = { 0.0, 5.76 };
    chart.meta.lineHeightUnit = { 0.0, 0.0075 };
    chart.meta.worldOrigin = { 0.0, 1.0 };
    chart.meta.worldViewport = { 1.0, -1.0 };

    if (1 <= formatVersion && formatVersion <= 3) {
        if (!jsonRoot.hasKey("offset")) CHART_LOAD_FAILED("official", "missing offset field");
        if (!jsonRoot["offset"].isNumber()) CHART_LOAD_FAILED("official", "offset is not a number");
        chart.meta.offset = jsonRoot["offset"].getNumber();

        if (!jsonRoot.hasKey("judgeLineList")) CHART_LOAD_FAILED("official", "missing judgeLineList field");
        if (!jsonRoot["judgeLineList"].isArray()) CHART_LOAD_FAILED("official", "judgeLineList is not an array");
        auto& judgeLineListNode = jsonRoot["judgeLineList"];

        for (auto& judgeLineNode : judgeLineListNode.getArray()) {
            if (!judgeLineNode.isObject()) CHART_LOAD_FAILED("official", "judgeLineList item is not an object");
            
            auto& line = chart.lines.emplace_back();
            line.enableCover = true;

            if (!judgeLineNode.hasKey("bpm")) CHART_LOAD_FAILED("official", "missing bpm field");
            if (!judgeLineNode["bpm"].isNumber()) CHART_LOAD_FAILED("official", "bpm is not a number");
            ep_f64 bpm = judgeLineNode["bpm"].getNumber();
            ep_f64 timeFactor = 1.875 / bpm;
            line.bpms = { { 0, bpm } };

            if (!judgeLineNode.hasKey("notesAbove")) CHART_LOAD_FAILED("official", "missing notesAbove field");
            if (!judgeLineNode["notesAbove"].isArray()) CHART_LOAD_FAILED("official", "notesAbove is not an array");
            if (!judgeLineNode.hasKey("notesBelow")) CHART_LOAD_FAILED("official", "missing notesBelow field");
            if (!judgeLineNode["notesBelow"].isArray()) CHART_LOAD_FAILED("official", "notesBelow is not an array");

            auto& notesAboveNode = judgeLineNode["notesAbove"];
            auto& notesBelowNode = judgeLineNode["notesBelow"];
            std::vector<std::pair<JsonNode*, bool>> noteGroups = {
                { &notesAboveNode, true },
                { &notesBelowNode, false }
            };

            for (auto& [ notesNodePtr, isAbove ] : noteGroups) {
                auto& notesNode = *notesNodePtr;

                for (auto& noteNode : notesNode.getArray()) {
                    if (!noteNode.isObject()) CHART_LOAD_FAILED("official", "notesAbove/notesBelow item is not an object");

                    if (!noteNode.hasKey("type")) CHART_LOAD_FAILED("official", "missing type field");
                    if (!noteNode["type"].isNumber()) CHART_LOAD_FAILED("official", "type is not a number");
                    auto type = PhiNoteTypeHelper::FromOfficial(noteNode["type"].getNumber());

                    if (!noteNode.hasKey("time")) CHART_LOAD_FAILED("official", "missing time field");
                    if (!noteNode["time"].isNumber()) CHART_LOAD_FAILED("official", "time is not a number");
                    auto time = noteNode["time"].getNumber() * timeFactor;

                    if (!noteNode.hasKey("holdTime")) CHART_LOAD_FAILED("official", "missing holdTime field");
                    if (!noteNode["holdTime"].isNumber()) CHART_LOAD_FAILED("official", "holdTime is not a number");
                    auto holdTime = noteNode["holdTime"].getNumber() * timeFactor;

                    if (!noteNode.hasKey("positionX")) CHART_LOAD_FAILED("official", "missing positionX field");
                    if (!noteNode["positionX"].isNumber()) CHART_LOAD_FAILED("official", "positionX is not a number");
                    auto positionX = noteNode["positionX"].getNumber() * 0.05625;

                    std::string speedKey = "speed";
                    if (!noteNode.hasKey(speedKey)) CHART_LOAD_FAILED("official", std::string("missing ") + speedKey + " field");
                    if (!noteNode[speedKey].isNumber()) CHART_LOAD_FAILED("official", speedKey + " is not a number");
                    auto speed = noteNode[speedKey].getNumber();

                    auto& note = line.notes.emplace_back();
                    note.type = type;
                    note.time = time;
                    note.holdTime = holdTime;
                    note.isFake = false;

                    chart.animator.addEvent(note, PhiEvent {
                        .timeZone = INF_TZ,
                        .valueZone = { positionX, positionX },
                        .type = EnumPhiEventType::PositionX,
                        .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                    });

                    if (type == EnumPhiNoteType::Hold) {
                        note.fixedHoldSpeed = speed * 0.6;
                    } else {
                        if (speed != 1.0) {
                            chart.animator.addEvent(note, PhiEvent {
                                .timeZone = INF_TZ,
                                .valueZone = { speed, speed },
                                .type = EnumPhiEventType::SpeedCoefficient,
                                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                            });
                        }
                    }

                    if (!isAbove) {
                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = INF_TZ,
                            .valueZone = { -1.0, -1.0 },
                            .type = EnumPhiEventType::SpeedCoefficient,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
                        });

                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = INF_TZ,
                            .valueZone = { 180.0, 180.0 },
                            .type = EnumPhiEventType::SelfRotation,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
                        });

                        note.reverseCover();
                    }
                }
            }

            if (!judgeLineNode.hasKey("speedEvents")) CHART_LOAD_FAILED("official", "missing speedEvents field");
            if (!judgeLineNode["speedEvents"].isArray()) CHART_LOAD_FAILED("official", "speedEvents is not an array");
            if (!judgeLineNode.hasKey("judgeLineMoveEvents")) CHART_LOAD_FAILED("official", "missing judgeLineMoveEvents field");
            if (!judgeLineNode["judgeLineMoveEvents"].isArray()) CHART_LOAD_FAILED("official", "judgeLineMoveEvents is not an array");
            if (!judgeLineNode.hasKey("judgeLineRotateEvents")) CHART_LOAD_FAILED("official", "missing judgeLineRotateEvents field");
            if (!judgeLineNode["judgeLineRotateEvents"].isArray()) CHART_LOAD_FAILED("official", "judgeLineRotateEvents is not an array");
            if (!judgeLineNode.hasKey("judgeLineDisappearEvents")) CHART_LOAD_FAILED("official", "missing judgeLineDisappearEvents field");
            if (!judgeLineNode["judgeLineDisappearEvents"].isArray()) CHART_LOAD_FAILED("official", "judgeLineDisappearEvents is not an array");

            auto& speedEventsNode = judgeLineNode["speedEvents"];
            auto& judgeLineMoveEventsNode = judgeLineNode["judgeLineMoveEvents"];
            auto& judgeLineRotateEventsNode = judgeLineNode["judgeLineRotateEvents"];
            auto& judgeLineDisappearEventsNode = judgeLineNode["judgeLineDisappearEvents"];

            // events, startKey, endKey, easeTypeKey, type, converter
            std::vector<std::tuple<JsonNode*, std::string, std::string, std::string, EnumPhiEventType, std::function<ep_f64(ep_f64)>>> eventGroups;
            
            if (formatVersion == 1) {
                eventGroups = {
                    { &speedEventsNode, "value", "value", "", EnumPhiEventType::Speed, [](ep_f64 v) { return v * 0.6; } },
                    { &judgeLineMoveEventsNode, "start", "end", "", EnumPhiEventType::PositionX, [](ep_f64 v) { return std::floor(v / 1000.0) / 880.0; } },
                    { &judgeLineMoveEventsNode, "start", "end", "", EnumPhiEventType::PositionY, [](ep_f64 v) { return std::fmod(v, 1000.0) / 520.0; } },
                    { &judgeLineRotateEventsNode, "start", "end", "", EnumPhiEventType::SelfRotation, [](ep_f64 v) { return -v; } },
                    { &judgeLineDisappearEventsNode, "start", "end", "", EnumPhiEventType::AdditiveAlpha, [](ep_f64 v) { return v; } }
                };
            } else if (formatVersion == 3) {
                eventGroups = {
                    { &speedEventsNode, "value", "value", "", EnumPhiEventType::Speed, [](ep_f64 v) { return v * 0.6; } },
                    { &judgeLineMoveEventsNode, "start", "end", "", EnumPhiEventType::PositionX, [](ep_f64 v) { return v; } },
                    { &judgeLineMoveEventsNode, "start2", "end2", "", EnumPhiEventType::PositionY, [](ep_f64 v) { return v; } },
                    { &judgeLineRotateEventsNode, "start", "end", "", EnumPhiEventType::SelfRotation, [](ep_f64 v) { return -v; } },
                    { &judgeLineDisappearEventsNode, "start", "end", "", EnumPhiEventType::AdditiveAlpha, [](ep_f64 v) { return v; } }
                };
            }

            for (auto& [eventsNode, startKey, endKey, easeTypeKey, type, converter] : eventGroups) {
                if (!eventsNode->isArray()) CHART_LOAD_FAILED("official", "XXXEvents is not an array");

                for (auto& eventNode : eventsNode->getArray()) {
                    if (!eventNode.isObject()) CHART_LOAD_FAILED("official", "XXXEvents item is not an object");

                    if (!eventNode.hasKey("startTime")) CHART_LOAD_FAILED("official", "missing startTime field");
                    if (!eventNode["startTime"].isNumber()) CHART_LOAD_FAILED("official", "startTime is not a number");
                    auto startTime = eventNode["startTime"].getNumber() * timeFactor;

                    if (!eventNode.hasKey("endTime")) CHART_LOAD_FAILED("official", "missing endTime field");
                    if (!eventNode["endTime"].isNumber()) CHART_LOAD_FAILED("official", "endTime is not a number");
                    auto endTime = eventNode["endTime"].getNumber() * timeFactor;

                    if (!eventNode.hasKey(startKey)) CHART_LOAD_FAILED("official", std::string("missing ") + startKey + " field");
                    if (!eventNode[startKey].isNumber()) CHART_LOAD_FAILED("official", std::string(startKey) + " is not a number");
                    auto startValue = converter(eventNode[startKey].getNumber());

                    if (!eventNode.hasKey(endKey)) CHART_LOAD_FAILED("official", std::string("missing ") + endKey + " field");
                    if (!eventNode[endKey].isNumber()) CHART_LOAD_FAILED("official", std::string(endKey) + " is not a number");
                    auto endValue = converter(eventNode[endKey].getNumber());

                    chart.animator.addEvent(line, PhiEvent {
                        .timeZone = { startTime, endTime },
                        .valueZone = { startValue, endValue },
                        .type = type,
                        .layerIndex = PhiEventLayerIndexs::LINE_DEFAULT
                    });
                }
            }
        }
    } else {
        CHART_LOAD_FAILED("official", std::string("unsupported formatVersion: ") + std::to_string(formatVersion))
    }

    chart.rawHash = data.getHash();

    return PhiChartLoadResult {
        .success = true,
        .chart = chart
    };
}

PhiChartLoadResult loadPhiChartFromRpeJson(const Data& data) {
    /* !docs
    Loads a phigros chart from a Re:PhiEdit JSON file.
    */

    JsonNode jsonRoot;
    auto [jsonParseSuccess, err] = JsonNode::Parse(&jsonRoot, data);
    if (!jsonParseSuccess) CHART_LOAD_FAILED("rpe", std::string("failed to parse json: ") + err);
    
    PhiChart chart {};

    chart.meta.isHoldCoverAtHead = false;
    chart.meta.isZeroLengthHoldHidden = false;
    chart.meta.isHighNoteHidden = false;
    chart.meta.isRegLineAlphaNoteHidden = true;
    chart.meta.lineWidthUnit = { (ep_f64)4000 / 1350, 0.0 };
    chart.meta.lineHeightUnit = { 0.0, (ep_f64)1 / 180 };
    chart.meta.worldOrigin = { (ep_f64)-1350 / 2, (ep_f64)900 / 2 };
    chart.meta.worldViewport = { 1350, -900 };

    if (!jsonRoot.isObject()) CHART_LOAD_FAILED("rpe", "root is not an object");

    if (!jsonRoot.hasKey("META")) CHART_LOAD_FAILED("rpe", "missing META field");
    if (!jsonRoot["META"].isObject()) CHART_LOAD_FAILED("rpe", "META is not an object");
    auto& metaNode = jsonRoot["META"];

    if (!metaNode.hasKey("RPEVersion")) CHART_LOAD_FAILED("rpe", "missing RPEVersion field");
    if (!metaNode["RPEVersion"].isNumber()) CHART_LOAD_FAILED("rpe", "RPEVersion is not a number");
    chart.meta.rpeVersion = metaNode["RPEVersion"].getNumber();

    if (!metaNode.hasKey("charter")) CHART_LOAD_FAILED("rpe", "missing charter field");
    if (!metaNode["charter"].isString()) CHART_LOAD_FAILED("rpe", "charter is not a string");
    chart.meta.charter = metaNode["charter"].getString();

    if (!metaNode.hasKey("composer")) CHART_LOAD_FAILED("rpe", "missing composer field");
    if (!metaNode["composer"].isString()) CHART_LOAD_FAILED("rpe", "composer is not a string");
    chart.meta.composer = metaNode["composer"].getString();

    if (!metaNode.hasKey("name")) CHART_LOAD_FAILED("rpe", "missing name field");
    if (!metaNode["name"].isString()) CHART_LOAD_FAILED("rpe", "name is not a string");
    chart.meta.title = metaNode["name"].getString();

    if (!metaNode.hasKey("level")) CHART_LOAD_FAILED("rpe", "missing level field");
    if (!metaNode["level"].isString()) CHART_LOAD_FAILED("rpe", "level is not a string");
    chart.meta.difficulty = metaNode["level"].getString();

    if (!metaNode.hasKey("offset")) CHART_LOAD_FAILED("rpe", "missing offset field");
    if (!metaNode["offset"].isNumber()) CHART_LOAD_FAILED("rpe", "offset is not a number");
    chart.meta.offset = metaNode["offset"].getNumber() / 1000;

    auto parseTimeTuple = [](const JsonNode& node, ep_f64* dst) {
        if (!node.isArray()) return false;
        if (node.getArray().size() != 3) return false;

        const auto& arr = node.getArray();
        if (!arr[0].isNumber()) return false;
        if (!arr[1].isNumber()) return false;
        if (!arr[2].isNumber()) return false;

        ep_f64 n1 = arr[0].getNumber(),
               n2 = arr[1].getNumber(),
               n3 = arr[2].getNumber();

        *dst = n1 + n2 / n3;
        return true;
    };

    std::vector<PhiBPMEvent> sharedBpmEvents;
    
    if (!jsonRoot.hasKey("BPMList")) CHART_LOAD_FAILED("rpe", "missing BPMList field");
    if (!jsonRoot["BPMList"].isArray()) CHART_LOAD_FAILED("rpe", "BPMList is not an array");
    auto& bpmListNode = jsonRoot["BPMList"];

    for (auto& bpmEventNode : bpmListNode.getArray()) {
        if (!bpmEventNode.isObject()) CHART_LOAD_FAILED("rpe", "BPMList item is not an object");

        if (!bpmEventNode.hasKey("startTime")) CHART_LOAD_FAILED("rpe", "missing startTime field");
        ep_f64 startTime;
        if (!parseTimeTuple(bpmEventNode["startTime"], &startTime)) CHART_LOAD_FAILED("rpe", "startTime is not a valid time tuple");

        if (!bpmEventNode.hasKey("bpm")) CHART_LOAD_FAILED("rpe", "missing bpm field");
        if (!bpmEventNode["bpm"].isNumber()) CHART_LOAD_FAILED("rpe", "bpm is not a number");
        ep_f64 bpm = bpmEventNode["bpm"].getNumber();

        sharedBpmEvents.push_back({
            .time = startTime,
            .bpm = bpm
        });
    }

    PhiBPMEvent::SortBpmEvents(sharedBpmEvents);

    if (!jsonRoot.hasKey("judgeLineList")) CHART_LOAD_FAILED("rpe", "missing judgeLineList field");
    if (!jsonRoot["judgeLineList"].isArray()) CHART_LOAD_FAILED("rpe", "judgeLineList is not an array");
    auto& judgeLineListNode = jsonRoot["judgeLineList"];

    for (auto& judgeLineNode : judgeLineListNode.getArray()) {
        if (!judgeLineNode.isObject()) CHART_LOAD_FAILED("rpe", "judgeLineList item is not an object");

        auto& line = chart.lines.emplace_back();
        line.bpms = sharedBpmEvents;

        if (judgeLineNode.hasKey("bpmfactor")) {
            if (!judgeLineNode["bpmfactor"].isNumber()) CHART_LOAD_FAILED("rpe", "bpmfactor is not a number");
            auto factor = judgeLineNode["bpmfactor"].getNumber();

            for (auto& e : line.bpms) {
                e.bpm /= factor;
            }
        }

        if (!judgeLineNode.hasKey("eventLayers")) CHART_LOAD_FAILED("rpe", "missing eventLayers field");
        if (!judgeLineNode["eventLayers"].isArray()) CHART_LOAD_FAILED("rpe", "eventLayers is not an array");
        auto& eventLayersNode = judgeLineNode["eventLayers"];

        ep_u64 eventLayerIndex = 0;
        // events, type, converter
        using EventGroupType = std::tuple<JsonNode*, EnumPhiEventType, std::function<ep_f64(ep_f64)>>;

        auto progressEventGroup = [&](EventGroupType group) -> std::pair<bool, std::string> {
            auto& [eventsNode, type, converter] = group;
            if (!eventsNode->isArray()) return { false, "XXXEvents is not an array" };
            
            auto& arr =  eventsNode->getArray();
            if (arr.empty()) return { true, "" };

            ep_f64 earliestTime = INF_TIME;

            for (auto& eventNode : arr) {
                if (!eventNode.isObject()) return { false, "XXXEvents item is not an object" };

                if (!eventNode.hasKey("startTime")) return { false, "missing startTime field" };
                ep_f64 startTime;
                if (!parseTimeTuple(eventNode["startTime"], &startTime)) return { false, "startTime is not a valid time tuple" };

                if (!eventNode.hasKey("endTime")) return { false, "missing endTime field" };
                ep_f64 endTime;
                if (!parseTimeTuple(eventNode["endTime"], &endTime)) return { false, "endTime is not a valid time tuple" };

                ep_f64 start, end;

                if (!eventNode.hasKey("start")) return { false, "missing start field" };
                if (!eventNode.hasKey("end")) return { false, "missing end field" };

                if (type == EnumPhiEventType::Text) {
                    if (!eventNode["start"].isString()) return { false, "start is not a string" };
                    if (!eventNode["end"].isString()) return { false, "end is not a string" };

                    auto valueZone = chart.storyboardAssets.requestTextPair(eventNode["start"].getString(), eventNode["end"].getString());
                    start = valueZone.x;
                    end = valueZone.y;
                } else if (type == EnumPhiEventType::Color) {
                    if (!eventNode["start"].isArray()) return { false, "start is not an array" };
                    if (!eventNode["end"].isArray()) return { false, "end is not an array" };

                    auto& startArr = eventNode["start"].getArray();
                    auto& endArr = eventNode["end"].getArray();

                    if (startArr.size() < 3) return { false, "start array size is less than 3" };
                    if (endArr.size() < 3) return { false, "end array size is less than 3" };

                    if (!startArr[0].isNumber()) return { false, "start array item is not a number" };
                    if (!startArr[1].isNumber()) return { false, "start array item is not a number" };
                    if (!startArr[2].isNumber()) return { false, "start array item is not a number" };

                    if (!endArr[0].isNumber()) return { false, "end array item is not a number" };
                    if (!endArr[1].isNumber()) return { false, "end array item is not a number" };
                    if (!endArr[2].isNumber()) return { false, "end array item is not a number" };

                    auto startColor = Color {
                        startArr[0].getNumber() / 255,
                        startArr[1].getNumber() / 255,
                        startArr[2].getNumber() / 255,
                        1.0
                    };

                    auto endColor = Color {
                        endArr[0].getNumber() / 255,
                        endArr[1].getNumber() / 255,
                        endArr[2].getNumber() / 255,
                        1.0
                    };

                    auto valueZone = chart.storyboardAssets.requestColorPair(startColor, endColor);
                    start = valueZone.x;
                    end = valueZone.y;
                } else {
                    if (!eventNode["start"].isNumber()) return { false, "start is not a number" };
                    start = eventNode["start"].getNumber();

                    if (!eventNode["end"].isNumber()) return { false, "end is not a number" };
                    end = eventNode["end"].getNumber();
                }

                start = converter(start);
                end = converter(end);

                startTime = line.beat2sec(startTime);
                endTime = line.beat2sec(endTime);
                earliestTime = std::min(earliestTime, startTime);

                PhiEvent e {};
                e.timeZone = { startTime, endTime };
                e.valueZone = { start, end };
                e.type = type;
                e.layerIndex = PhiEventLayerIndexs::LINE_DEFAULT + eventLayerIndex;

                if (eventNode.hasKey("easingType") && eventNode["easingType"].isNumber()) {
                    e.easingFuncContext = (void*)(ep_u64)eventNode["easingType"].getNumber();
                }

                if ((ep_u64)e.easingFuncContext > 1) {
                    e.easingFunc = [](void* ctx, ep_f64 p) { return EaseSet::Phigros::RePhiEdit::easing((ep_u64)ctx, p); };
                    e.easingIntFunc = [](void* ctx, ep_f64 p) { return EaseSet::Phigros::RePhiEdit::easing_int((ep_u64)ctx, p); };
                }

                if (eventNode.hasKey("easingLeft")) {
                    if (!eventNode["easingLeft"].isNumber()) return { false, "easingLeft is not a number" };
                    e.easingZone.x = eventNode["easingLeft"].getNumber();
                }

                if (eventNode.hasKey("easingRight")) {
                    if (!eventNode["easingRight"].isNumber()) return { false, "easingRight is not a number" };
                    e.easingZone.y = eventNode["easingRight"].getNumber();
                }

                chart.animator.addEvent(line, e);
            }

            if (type == EnumPhiEventType::Text) {
                PhiEvent e {};
                e.timeZone = { -INF_TIME, earliestTime };
                e.valueZone = chart.storyboardAssets.requestTextPair("", "");
                e.type = type;
                e.layerIndex = PhiEventLayerIndexs::LINE_DEFAULT + eventLayerIndex;
                chart.animator.addEvent(line, e);
            }

            return { true, "" };
        };

        for (auto& eventLayerNode : eventLayersNode.getArray()) {
            if (eventLayerNode.isNull()) continue;
            if (!eventLayerNode.isObject()) CHART_LOAD_FAILED("rpe", "eventLayers item is not an object");

            std::vector<EventGroupType> groups;

            if (eventLayerNode.hasKey("alphaEvents")) groups.push_back({ &eventLayerNode["alphaEvents"], EnumPhiEventType::AdditiveAlpha, [](ep_f64 v) { return v / 255; } });
            if (eventLayerNode.hasKey("moveXEvents")) groups.push_back({ &eventLayerNode["moveXEvents"], EnumPhiEventType::PositionX, [](ep_f64 v) { return v; } });
            if (eventLayerNode.hasKey("moveYEvents")) groups.push_back({ &eventLayerNode["moveYEvents"], EnumPhiEventType::PositionY, [](ep_f64 v) { return v; } });
            if (eventLayerNode.hasKey("rotateEvents")) groups.push_back({ &eventLayerNode["rotateEvents"], EnumPhiEventType::SelfRotation, [](ep_f64 v) { return v; } });
            if (eventLayerNode.hasKey("speedEvents")) groups.push_back({ &eventLayerNode["speedEvents"], EnumPhiEventType::Speed, [](ep_f64 v) { return v * 120 / 900; } });

            for (auto& group : groups) {
                auto [success, msg] = progressEventGroup(group);
                if (!success) CHART_LOAD_FAILED("rpe", msg);
            }

            eventLayerIndex++;
        }

        if (judgeLineNode.hasKey("extended")) {
            auto& extendedNode = judgeLineNode["extended"];
            if (!extendedNode.isObject()) CHART_LOAD_FAILED("rpe", "extended is not an object");

            std::vector<EventGroupType> groups;

            if (extendedNode.hasKey("textEvents")) groups.push_back({ &extendedNode["textEvents"], EnumPhiEventType::Text, [](ep_f64 v) { return v; } });
            if (extendedNode.hasKey("scaleXEvents")) groups.push_back({ &extendedNode["scaleXEvents"], EnumPhiEventType::ScaleX, [](ep_f64 v) { return v; } });
            if (extendedNode.hasKey("scaleYEvents")) groups.push_back({ &extendedNode["scaleYEvents"], EnumPhiEventType::ScaleY, [](ep_f64 v) { return v; } });
            if (extendedNode.hasKey("colorEvents")) groups.push_back({ &extendedNode["colorEvents"], EnumPhiEventType::Color, [](ep_f64 v) { return v; } });

            for (auto& group : groups) {
                auto [success, msg] = progressEventGroup(group);
                if (!success) CHART_LOAD_FAILED("rpe", msg);
            }

            eventLayerIndex++;
        }

        if (judgeLineNode.hasKey("notes")) {
            auto& notesNode = judgeLineNode["notes"];
            if (!notesNode.isArray()) CHART_LOAD_FAILED("rpe", "notes is not an array");

            for (auto& noteNode : notesNode.getArray()) {
                if (!noteNode.isObject()) CHART_LOAD_FAILED("rpe", "notes item is not an object");

                auto& note = line.notes.emplace_back();

                if (!noteNode.hasKey("startTime")) CHART_LOAD_FAILED("rpe", "missing startTime field");
                ep_f64 startTime;
                if (!parseTimeTuple(noteNode["startTime"], &startTime)) CHART_LOAD_FAILED("rpe", "startTime is not a valid time tuple");

                if (!noteNode.hasKey("endTime")) CHART_LOAD_FAILED("rpe", "missing endTime field");
                ep_f64 endTime;
                if (!parseTimeTuple(noteNode["endTime"], &endTime)) CHART_LOAD_FAILED("rpe", "endTime is not a valid time tuple");

                startTime = line.beat2sec(startTime);
                endTime = line.beat2sec(endTime);

                if (!noteNode.hasKey("above")) CHART_LOAD_FAILED("rpe", "missing above field");
                bool isAbove;
                if (noteNode["above"].isBool()) isAbove = noteNode["above"].getBool();
                else if (noteNode["above"].isNumber()) isAbove = noteNode["above"].getNumber() == 1;
                else CHART_LOAD_FAILED("rpe", "above is not a boolean or number");

                if (!noteNode.hasKey("type")) CHART_LOAD_FAILED("rpe", "missing type field");
                if (!noteNode["type"].isNumber()) CHART_LOAD_FAILED("rpe", "type is not a number");
                auto type = PhiNoteTypeHelper::FromRPE(noteNode["type"].getNumber());

                if (!noteNode.hasKey("speed")) CHART_LOAD_FAILED("rpe", "missing speed field");
                if (!noteNode["speed"].isNumber()) CHART_LOAD_FAILED("rpe", "speed is not a number");
                ep_f64 speed = noteNode["speed"].getNumber();

                if (!noteNode.hasKey("isFake")) CHART_LOAD_FAILED("rpe", "missing isFake field");
                bool isFake;
                if (noteNode["isFake"].isBool()) isFake = noteNode["isFake"].getBool();
                else if (noteNode["isFake"].isNumber()) isFake = noteNode["isFake"].getNumber() == 1;
                else CHART_LOAD_FAILED("rpe", "isFake is not a boolean or number");

                if (!noteNode.hasKey("positionX")) CHART_LOAD_FAILED("rpe", "missing positionX field");
                if (!noteNode["positionX"].isNumber()) CHART_LOAD_FAILED("rpe", "positionX is not a number");
                ep_f64 positionX = noteNode["positionX"].getNumber() / 1350;
                
                if (noteNode.hasKey("yOffset")) {
                    if (!noteNode["yOffset"].isNumber()) CHART_LOAD_FAILED("rpe", "yOffset is not a number");
                    auto yOffset = noteNode["yOffset"].getNumber() / 900 * speed;
                    if (!isAbove) yOffset *= -1;
                    
                    if (yOffset != 0.0) {
                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = INF_TZ,
                            .valueZone = { yOffset, yOffset },
                            .type = EnumPhiEventType::PositionY,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                        });
                    }
                }

                if (noteNode.hasKey("visibleTime")) {
                    if (!noteNode["visibleTime"].isNumber()) CHART_LOAD_FAILED("rpe", "visibleTime is not a number");
                    auto visibleTime = noteNode["visibleTime"].getNumber();

                    if (visibleTime < 999999.0) {
                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = { -INF_TIME, startTime - visibleTime },
                            .valueZone = { 0.0, 0.0 },
                            .type = EnumPhiEventType::MultiplyAlpha,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                        });

                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = { startTime - visibleTime, INF_TIME },
                            .valueZone = { 1.0, 1.0 },
                            .type = EnumPhiEventType::MultiplyAlpha,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                        });
                    }
                }

                if (noteNode.hasKey("size")) {
                    if (!noteNode["size"].isNumber()) CHART_LOAD_FAILED("rpe", "size is not a number");
                    auto size = noteNode["size"].getNumber();

                    if (size != 1.0) {
                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = INF_TZ,
                            .valueZone = { size, size },
                            .type = EnumPhiEventType::ScaleX,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                        });
                    }
                }

                if (noteNode.hasKey("alpha")) {
                    if (!noteNode["alpha"].isNumber()) CHART_LOAD_FAILED("rpe", "alpha is not a number");
                    auto alpha = noteNode["alpha"].getNumber() / 255;

                    if (alpha != 1.0) {
                        chart.animator.addEvent(note, PhiEvent {
                            .timeZone = INF_TZ,
                            .valueZone = { alpha, alpha },
                            .type = EnumPhiEventType::MultiplyAlpha,
                            .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
                        });
                    }
                }

                if (noteNode.hasKey("tint")) {
                    if (!noteNode["tint"].isArray()) CHART_LOAD_FAILED("rpe", "tint is not an array");

                    auto& arr = noteNode["tint"].getArray();
                    if (arr.size() < 3) CHART_LOAD_FAILED("rpe", "tint array is too small");

                    auto& n1 = arr[0];
                    auto& n2 = arr[1];
                    auto& n3 = arr[2];

                    if (!n1.isNumber()) CHART_LOAD_FAILED("rpe", "tint[0] is not a number");
                    if (!n2.isNumber()) CHART_LOAD_FAILED("rpe", "tint[1] is not a number");
                    if (!n3.isNumber()) CHART_LOAD_FAILED("rpe", "tint[2] is not a number");

                    auto color = Color {
                        n1.getNumber() / 255,
                        n2.getNumber() / 255,
                        n3.getNumber() / 255,
                        1.0
                    };

                    chart.animator.addEvent(note, PhiEvent {
                        .timeZone = INF_TZ,
                        .valueZone = chart.storyboardAssets.requestColorPair(color, color),
                        .type = EnumPhiEventType::Color,
                        .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                    });
                }

                note.type = type;
                note.time = startTime;
                note.holdTime = endTime - startTime;
                note.isFake = isFake;

                chart.animator.addEvent(note, PhiEvent {
                    .timeZone = INF_TZ,
                    .valueZone = { positionX, positionX },
                    .type = EnumPhiEventType::PositionX,
                    .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                });

                if (speed != 1.0) {
                    chart.animator.addEvent(note, PhiEvent {
                        .timeZone = INF_TZ,
                        .valueZone = { speed, speed },
                        .type = EnumPhiEventType::SpeedCoefficient,
                        .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
                    });
                }

                if (!isAbove) {
                    chart.animator.addEvent(note, PhiEvent {
                        .timeZone = INF_TZ,
                        .valueZone = { -1.0, -1.0 },
                        .type = EnumPhiEventType::SpeedCoefficient,
                        .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
                    });

                    chart.animator.addEvent(note, PhiEvent {
                        .timeZone = INF_TZ,
                        .valueZone = { 180.0, 180.0 },
                        .type = EnumPhiEventType::SelfRotation,
                        .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
                    });

                    note.reverseCover();
                }
            }
        }

        if (judgeLineNode.hasKey("attachUI")) {
            if (!judgeLineNode["attachUI"].isString()) CHART_LOAD_FAILED("rpe", "attachUI is not a string");
            line.attachUI = PhiLineAttachUIHelper::FromString(judgeLineNode["attachUI"].getString());
        }

        if (judgeLineNode.hasKey("Texture")) {
            if (!judgeLineNode["Texture"].isString()) CHART_LOAD_FAILED("rpe", "Texture is not a string");
            auto textureName = judgeLineNode["Texture"].getString();

            if (textureName != "line.png") {
                line.textureName = textureName;
            }
        }

        if (judgeLineNode.hasKey("father")) {
            if (!judgeLineNode["father"].isNumber()) CHART_LOAD_FAILED("rpe", "father is not a number");
            ep_i64 fatherLineIndex = judgeLineNode["father"].getNumber();
            if (fatherLineIndex >= 0) {
                line.fatherLineIndex = fatherLineIndex;
            }
        }

        bool enableCover = true;
        if (judgeLineNode.hasKey("isCover")) {
            if (judgeLineNode["isCover"].isNumber()) enableCover = judgeLineNode["isCover"].getNumber() == 1;
            else if (judgeLineNode["isCover"].isBool()) enableCover = judgeLineNode["isCover"].getBool();
            else CHART_LOAD_FAILED("rpe", "isCover is not a boolean or number");
        }
        line.enableCover = enableCover;

        if (judgeLineNode.hasKey("zOrder")) {
            if (!judgeLineNode["zOrder"].isNumber()) CHART_LOAD_FAILED("rpe", "zOrder is not a number");
            line.zOrder = judgeLineNode["zOrder"].getNumber();
        }

        if (judgeLineNode.hasKey("anchor")) {
            if (!judgeLineNode["anchor"].isArray()) CHART_LOAD_FAILED("rpe", "anchor is not an array");

            auto& anchorArr = judgeLineNode["anchor"].getArray();
            if (anchorArr.size() < 2) CHART_LOAD_FAILED("rpe", "anchor array size is less than 2");

            if (!anchorArr[0].isNumber() || !anchorArr[1].isNumber()) CHART_LOAD_FAILED("rpe", "anchor array element is not a number");
            line.anchor = { anchorArr[0].getNumber(), anchorArr[1].getNumber() };
        }
    }

    chart.rawHash = data.getHash();

    return PhiChartLoadResult {
        .success = true,
        .chart = chart
    };
}

PhiChartLoadResult loadPhiChartFromPec(const Data& data) {
    /* !docs
    Loads a phigros chart from a PhiEdit Chart file (.pec).
    */

    struct TokenReader {
        std::string str;
        ep_u64 pos = 0;

        TokenReader(const std::string& str) : str(str) {}

        bool nextToken(std::string& dst) {
            jumpToNextNonWhiteSpace();
            if (pos >= str.size()) return false;
            ep_u64 start = pos;
            jumpToNextWhiteSpace();
            if (pos == start) return false;
            dst = str.substr(start, pos - start);
            jumpToNextNonWhiteSpace();
            return true;
        }

        private:
        bool currentIsWhiteSpace() const {
            return str[pos] == ' ' || str[pos] == '\t' || str[pos] == '\n' || str[pos] == '\r' || str[pos] == '\f' || str[pos] == '\v';
        }

        void jumpToNextWhiteSpace() {
            while (pos < str.size() && !currentIsWhiteSpace()) pos++;
        }

        void jumpToNextNonWhiteSpace() {
            while (pos < str.size() && currentIsWhiteSpace()) pos++;
        }
    };

    TokenReader reader(std::string((char*)data.data.data(), data.data.size()));
    std::string token;

    auto readNumber = [&](ep_f64* dst) {
        if (!reader.nextToken(token)) return false;
        char* end;
        *dst = std::strtod(token.c_str(), &end);
        return end != token.c_str();
    };

    auto readBool = [&](bool* dst) {
        ep_f64 num;
        if (!readNumber(&num)) return false;
        *dst = num == 1.0;
        return true;
    };

    PhiChart chart {};
    chart.meta.isHoldCoverAtHead = true;
    chart.meta.isZeroLengthHoldHidden = false;
    chart.meta.isHighNoteHidden = false;
    chart.meta.isRegLineAlphaNoteHidden = true;
    chart.meta.lineWidthUnit = { (ep_f64)4000 / 1350, 0.0 };
    chart.meta.lineHeightUnit = { 0.0, (ep_f64)1 / 180 };
    chart.meta.worldOrigin = { (ep_f64)-1350 / 2, (ep_f64)900 / 2 };
    chart.meta.worldViewport = { 1350, -900 };

    ep_f64 offset;
    if (!readNumber(&offset)) CHART_LOAD_FAILED("pec", "failed to read offset");
    chart.meta.offset = (offset - 150.0) / 1000.0;

    struct Commands {
        struct Bpm {
            ep_f64 startTime, bpm;
        };

        struct Note {
            ep_i64 lineIndex;
            PhiNote note;
            bool isAbove;
            ep_f64 speed = 1.0, size = 1.0, positionX;
        };

        struct Event {
            Vec2 timeZone;
            ep_f64 value;
            bool useFront = false;
            ep_u64 easingType = 1;
        };
    };

    std::vector<Commands::Bpm> bpmCommands;
    std::vector<Commands::Note> noteCommands;
    std::unordered_map<ep_i64, std::unordered_map<EnumPhiEventType, std::vector<Commands::Event>>> eventCommands;

    while (reader.nextToken(token)) {
        if (token == "bp") {
            ep_f64 startTime, bpm;
            if (!readNumber(&startTime)) CHART_LOAD_FAILED("pec", "failed to read startTime (bp)");
            if (!readNumber(&bpm)) CHART_LOAD_FAILED("pec", "failed to read bpm (bp)");

            bpmCommands.push_back({
                .startTime = startTime,
                .bpm = bpm
            });
        } else if (token == "n1" || token == "n2" || token == "n3" || token == "n4") {
            auto type = PhiNoteTypeHelper::FromPEC(token);

            ep_f64 lineIndex;
            if (!readNumber(&lineIndex)) CHART_LOAD_FAILED("pec", "failed to read lineIndex (nx)");

            ep_f64 startTime, endTime;
            if (!readNumber(&startTime)) CHART_LOAD_FAILED("pec", "failed to read startTime (nx)");
            if (type == EnumPhiNoteType::Hold) {
                if (!readNumber(&endTime)) CHART_LOAD_FAILED("pec", "failed to read endTime (nx)");
            } else endTime = startTime;

            ep_f64 positionX;
            bool isAbove, isFake;

            if (!readNumber(&positionX)) CHART_LOAD_FAILED("pec", "failed to read positionX (nx)");
            if (!readBool(&isAbove)) CHART_LOAD_FAILED("pec", "failed to read isAbove (nx)");
            if (!readBool(&isFake)) CHART_LOAD_FAILED("pec", "failed to read isFake (nx)");

            noteCommands.push_back(Commands::Note {
                .lineIndex = (ep_i64)lineIndex,
                .note = PhiNote {
                    .type = type,
                    .time = startTime,
                    .holdTime = endTime - startTime,
                    .isFake = isFake
                },
                .isAbove = isAbove,
                .positionX = positionX
            });
        } else if (token == "#") {
            ep_f64 speed;
            if (!readNumber(&speed)) CHART_LOAD_FAILED("pec", "failed to read speed (#)");
            if (!noteCommands.empty()) noteCommands.back().speed = speed;
        } else if (token == "&") {
            ep_f64 size;
            if (!readNumber(&size)) CHART_LOAD_FAILED("pec", "failed to read size (&)");
            if (!noteCommands.empty()) noteCommands.back().size = size;
        } else if (token == "cp") {
            ep_f64 lineIndex;
            if (!readNumber(&lineIndex)) CHART_LOAD_FAILED("pec", "failed to read lineIndex (cp)");

            ep_f64 time;
            if (!readNumber(&time)) CHART_LOAD_FAILED("pec", "failed to read time (cp)");

            ep_f64 x, y;
            if (!readNumber(&x)) CHART_LOAD_FAILED("pec", "failed to read x (cp)");
            if (!readNumber(&y)) CHART_LOAD_FAILED("pec", "failed to read y (cp)");

            eventCommands[lineIndex][EnumPhiEventType::PositionX].push_back(Commands::Event {
                .timeZone = { time, time }, .value = x
            });

            eventCommands[lineIndex][EnumPhiEventType::PositionY].push_back(Commands::Event {
                .timeZone = { time, time }, .value = y
            });
        } else if (token == "cd") {
            ep_f64 lineIndex;
            if (!readNumber(&lineIndex)) CHART_LOAD_FAILED("pec", "failed to read lineIndex (cd)");

            ep_f64 time;
            if (!readNumber(&time)) CHART_LOAD_FAILED("pec", "failed to read time (cd)");

            ep_f64 r;
            if (!readNumber(&r)) CHART_LOAD_FAILED("pec", "failed to read y (cd)");

            eventCommands[lineIndex][EnumPhiEventType::SelfRotation].push_back(Commands::Event {
                .timeZone = { time, time }, .value = r
            });
        } else if (token == "ca") {
            ep_f64 lineIndex;
            if (!readNumber(&lineIndex)) CHART_LOAD_FAILED("pec", "failed to read lineIndex (ca)");

            ep_f64 time;
            if (!readNumber(&time)) CHART_LOAD_FAILED("pec", "failed to read time (ca)");

            ep_f64 a;
            if (!readNumber(&a)) CHART_LOAD_FAILED("pec", "failed to read a (ca)");

            eventCommands[lineIndex][EnumPhiEventType::AdditiveAlpha].push_back(Commands::Event {
                .timeZone = { time, time }, .value = a
            });
        } else if (token == "cv") {
            ep_f64 lineIndex;
            if (!readNumber(&lineIndex)) CHART_LOAD_FAILED("pec", "failed to read lineIndex (cv)");

            ep_f64 time;
            if (!readNumber(&time)) CHART_LOAD_FAILED("pec", "failed to read time (cv)");

            ep_f64 v;
            if (!readNumber(&v)) CHART_LOAD_FAILED("pec", "failed to read v (cv)");

            eventCommands[lineIndex][EnumPhiEventType::Speed].push_back(Commands::Event {
                .timeZone = { time, time }, .value = v
            });
        } else if (token == "cm") {
            ep_f64 lineIndex;
            if (!readNumber(&lineIndex)) CHART_LOAD_FAILED("pec", "failed to read lineIndex (cm)");

            ep_f64 startTime, endTime;
            if (!readNumber(&startTime)) CHART_LOAD_FAILED("pec", "failed to read startTime (cm)");
            if (!readNumber(&endTime)) CHART_LOAD_FAILED("pec", "failed to read endTime (cm)");

            ep_f64 x, y;
            if (!readNumber(&x)) CHART_LOAD_FAILED("pec", "failed to read x (cm)");
            if (!readNumber(&y)) CHART_LOAD_FAILED("pec", "failed to read y (cm)");

            ep_f64 easingType;
            if (!readNumber(&easingType)) CHART_LOAD_FAILED("pec", "failed to read easingType (cm)");

            eventCommands[lineIndex][EnumPhiEventType::PositionX].push_back(Commands::Event {
                .timeZone = { startTime, endTime }, .value = x,
                .useFront = true, .easingType = (ep_u64)easingType
            });

            eventCommands[lineIndex][EnumPhiEventType::PositionY].push_back(Commands::Event {
                .timeZone = { startTime, endTime }, .value = y,
                .useFront = true, .easingType = (ep_u64)easingType
            });
        } else if (token == "cr") {
            ep_f64 lineIndex;
            if (!readNumber(&lineIndex)) CHART_LOAD_FAILED("pec", "failed to read lineIndex (cr)");

            ep_f64 startTime, endTime;
            if (!readNumber(&startTime)) CHART_LOAD_FAILED("pec", "failed to read startTime (cr)");
            if (!readNumber(&endTime)) CHART_LOAD_FAILED("pec", "failed to read endTime (cr)");

            ep_f64 r;
            if (!readNumber(&r)) CHART_LOAD_FAILED("pec", "failed to read r (cr)");

            ep_f64 easingType;
            if (!readNumber(&easingType)) CHART_LOAD_FAILED("pec", "failed to read easingType (cr)");

            eventCommands[lineIndex][EnumPhiEventType::SelfRotation].push_back(Commands::Event {
                .timeZone = { startTime, endTime }, .value = r,
                .useFront = true, .easingType = (ep_u64)easingType
            });
        } else if (token == "cf") {
            ep_f64 lineIndex;
            if (!readNumber(&lineIndex)) CHART_LOAD_FAILED("pec", "failed to read lineIndex (cf)");

            ep_f64 startTime, endTime;
            if (!readNumber(&startTime)) CHART_LOAD_FAILED("pec", "failed to read startTime (cf)");
            if (!readNumber(&endTime)) CHART_LOAD_FAILED("pec", "failed to read endTime (cf)");

            ep_f64 a;
            if (!readNumber(&a)) CHART_LOAD_FAILED("pec", "failed to read a (cf)");

            eventCommands[lineIndex][EnumPhiEventType::AdditiveAlpha].push_back(Commands::Event {
                .timeZone = { startTime, endTime }, .value = a,
                .useFront = true
            });
        }
    }

    std::sort(bpmCommands.begin(), bpmCommands.end(), [](const auto& a, const auto& b) { return a.startTime < b.startTime; });
    std::vector<PhiBPMEvent> sharedBpmEvents;
    for (auto& cmd : bpmCommands) {
        sharedBpmEvents.push_back(PhiBPMEvent {
            .time = cmd.startTime,
            .bpm = cmd.bpm
        });
    }

    std::unordered_map<ep_i64, ep_u64> lineIndexMap;
    auto getLineByIndex = [&](ep_i64 index) -> PhiLine& {
        if (lineIndexMap.contains(index)) return chart.lines[lineIndexMap[index]];
        PhiLine line {};
        line.bpms = sharedBpmEvents;
        chart.lines.push_back(line);
        lineIndexMap[index] = chart.lines.size() - 1;
        return chart.lines.back();
    };

    auto toSeconds = [&](ep_i64 lineIndex, ep_f64 beatTime) {
        auto& line = getLineByIndex(lineIndex);
        return line.beat2sec(beatTime);
    };

    for (auto& cmd : noteCommands) {
        auto& line = getLineByIndex(cmd.lineIndex);
        auto& note = line.notes.emplace_back(std::move(cmd.note));

        auto time = toSeconds(cmd.lineIndex, note.time);
        auto holdTime = toSeconds(cmd.lineIndex, note.time + note.holdTime) - time;
        note.time = time;
        note.holdTime = holdTime;

        if (!cmd.isAbove) {
            chart.animator.addEvent(note, PhiEvent {
                .timeZone = INF_TZ,
                .valueZone = { -1.0, -1.0 },
                .type = EnumPhiEventType::SpeedCoefficient,
                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
            });

            chart.animator.addEvent(note, PhiEvent {
                .timeZone = INF_TZ,
                .valueZone = { 180.0, 180.0 },
                .type = EnumPhiEventType::SelfRotation,
                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS_2
            });

            note.reverseCover();
        }

        if (cmd.positionX != 0.0) {
            auto value = cmd.positionX / 2048.0;

            chart.animator.addEvent(note, PhiEvent {
                .timeZone = INF_TZ,
                .valueZone = { value, value },
                .type = EnumPhiEventType::PositionX,
                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
            });
        }

        if (cmd.speed != 1.0) {
            chart.animator.addEvent(note, PhiEvent {
                .timeZone = INF_TZ,
                .valueZone = { cmd.speed, cmd.speed },
                .type = EnumPhiEventType::SpeedCoefficient,
                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
            });
        }

        if (cmd.size != 1.0) {
            chart.animator.addEvent(note, PhiEvent {
                .timeZone = INF_TZ,
                .valueZone = { cmd.size, cmd.size },
                .type = EnumPhiEventType::ScaleX,
                .layerIndex = PhiEventLayerIndexs::NOTE_ATTRS
            });
        }
    }

    for (auto& [lineIndex, events] : eventCommands) {
        for (auto& [type, typedEvents] : events) {
            std::sort(typedEvents.begin(), typedEvents.end(), [](const auto& a, const auto& b) {
                if (a.timeZone.x != b.timeZone.x) return a.timeZone.x < b.timeZone.x;
                if (a.timeZone.y != b.timeZone.y) return a.timeZone.y < b.timeZone.y;
                return b.useFront;
            });

            for (auto& cmd : typedEvents) {
                cmd.timeZone.x = toSeconds(lineIndex, cmd.timeZone.x);
                cmd.timeZone.y = toSeconds(lineIndex, cmd.timeZone.y);

                if (type == EnumPhiEventType::PositionX) cmd.value = (cmd.value / 2048.0 - 0.5) * 1350.0;
                else if (type == EnumPhiEventType::PositionY) cmd.value = (cmd.value / 1400.0 - 0.5) * 900.0;
                else if (type == EnumPhiEventType::Speed) cmd.value = cmd.value / 1400.0 * 900.0 * 120.0 / 900.0;
                else if (type == EnumPhiEventType::AdditiveAlpha) cmd.value /= 255.0;
            }

            for (ep_u64 i = 0; i < typedEvents.size(); i++) {
                auto& cmd = typedEvents[i];
                auto& line = getLineByIndex(lineIndex);

                ep_f64 startValue;
                if (cmd.useFront && i - 1 >= 0) startValue = typedEvents[i - 1].value;
                else startValue = cmd.value;

                if (i < typedEvents.size() - 1) {
                    auto& next = typedEvents[i + 1];
                    if (cmd.timeZone.isZeroZone() && next.timeZone.x == cmd.timeZone.y) continue;
                }
                
                if (cmd.timeZone.isZeroZone()) cmd.timeZone.y += 1.0;

                PhiEvent e {};
                e.timeZone = cmd.timeZone;
                e.valueZone = { startValue, cmd.value };
                e.type = type;
                e.layerIndex = PhiEventLayerIndexs::LINE_DEFAULT;

                if (cmd.easingType > 1) {
                    e.easingFuncContext = (void*)cmd.easingType;
                    e.easingFunc = [](void* ctx, ep_f64 p) { return EaseSet::Phigros::RePhiEdit::easing((ep_u64)ctx, p); };
                    e.easingIntFunc = [](void* ctx, ep_f64 p) { return EaseSet::Phigros::RePhiEdit::easing_int((ep_u64)ctx, p); };
                }

                chart.animator.addEvent(line, e);
            }
        }
    }

    chart.rawHash = data.getHash();

    return PhiChartLoadResult {
        .success = true,
        .chart = chart
    };
}

PhiChartLoadResult loadPhiChartFromData(const Data& data) {
    /* !docs
    Loads a Phi chart from a data object.
    
    It will try to load the chart from different formats in order:

    - Official Json
    - Re:PhiEdit Json
    - PhiEdit Chart (pec)
    */

    PhiChartLoadResult result {};
    result.success = false;

    #define TRY_LOAD_FUNC(func) \
        { \
            auto res = func(data); \
            if (res.success) return res; \
            result.errors.insert(result.errors.end(), res.errors.begin(), res.errors.end()); \
        }
    
    TRY_LOAD_FUNC(loadPhiChartFromOfficialJson);
    TRY_LOAD_FUNC(loadPhiChartFromRpeJson);
    TRY_LOAD_FUNC(loadPhiChartFromPec);

    return result;

    #undef TRY_LOAD_FUNC
}

#undef CHART_LOAD_FAILED

std::variant<PhiExtra, std::string> loadPhiExtraFromJsonData(const Data& data, PhiStoryboardAssets& assets) {
    /* !docs
    Loads extra from a data object.
    */

    JsonNode jsonRoot;
    auto [jsonParseSuccess, err] = JsonNode::Parse(&jsonRoot, data);
    if (!jsonParseSuccess) return std::string("failed to parse json: ") + err;

    if (!jsonRoot.isObject()) return "root is not an object";

    PhiExtra extra {};
    
    auto parseTimeTuple = [](const JsonNode& node, ep_f64* dst) {
        if (!node.isArray()) return false;
        if (node.getArray().size() != 3) return false;

        const auto& arr = node.getArray();
        if (!arr[0].isNumber()) return false;
        if (!arr[1].isNumber()) return false;
        if (!arr[2].isNumber()) return false;

        ep_f64 n1 = arr[0].getNumber(),
               n2 = arr[1].getNumber(),
               n3 = arr[2].getNumber();

        *dst = n1 + n2 / n3;
        return true;
    };

    std::vector<PhiBPMEvent> bpmEvents;

    if (!jsonRoot.hasKey("bpm")) return "missing bpm field";
    if (!jsonRoot["bpm"].isArray()) return "bpm is not an array";

    auto& bpmArr = jsonRoot["bpm"].getArray();
    for (auto& bpmEventNode : bpmArr) {
        if (!bpmEventNode.isObject()) return "bpm item is not an object";

        if (!bpmEventNode.hasKey("time")) return "missing time field";
        ep_f64 time;
        if (!parseTimeTuple(bpmEventNode["time"], &time)) return "time is not a valid time tuple";

        if (!bpmEventNode.hasKey("bpm")) return "missing bpm field";
        if (!bpmEventNode["bpm"].isNumber()) return "bpm is not a number";
        ep_f64 bpm = bpmEventNode["bpm"].getNumber();

        bpmEvents.push_back({
            .time = time,
            .bpm = bpm
        });
    }

    PhiBPMEvent::SortBpmEvents(bpmEvents);

    PhiLine tempLine {};
    tempLine.bpms = bpmEvents;

    auto parseTimeTupleToSecond = [&](const JsonNode& node, ep_f64* dst) {
        if (!parseTimeTuple(node, dst)) return false;
        *dst = tempLine.beat2sec(*dst);
        return true;
    };

    auto parseVectorUniform = [&](JsonNode& node, PhiShaderUniform* dst) {
        if (!node.isArray()) return false;
        auto& arr = node.getArray();
        for (auto& i : arr) {
            if (!i.isNumber()) return false;
        }

        if (!(2 <= arr.size() && arr.size() <= 4)) return false;

        dst->used = arr.size();

        for (ep_u8 i = 0; i < dst->used; i++) {
            dst->value[i] = arr[i].getNumber();
            if (dst->used >= 3) dst->value[i] /= 255.0;
        }

        return true;
    };

    if (!jsonRoot.hasKey("effects")) return "missing effects field";
    if (!jsonRoot["effects"].isArray()) return "effects is not an array";
    auto& effectsNode = jsonRoot["effects"].getArray();

    for (auto& effectNode : effectsNode) {
        if (!effectNode.isObject()) return "effects item is not an object";

        if (!effectNode.hasKey("start")) return "missing start field";
        if (!effectNode.hasKey("end")) return "missing end field";
        
        ep_f64 startTime, endTime;
        if (!parseTimeTupleToSecond(effectNode["start"], &startTime)) return "start is not a valid time tuple";
        if (!parseTimeTupleToSecond(effectNode["end"], &endTime)) return "end is not a valid time tuple";

        bool isGlobal = false;
        if (effectNode.hasKey("global")) {
            if (!effectNode["global"].isBool()) return "global is not a bool";
            isGlobal = effectNode["global"].getBool();
        }

        std::optional<ep_u64> targetLine;
        if (effectNode.hasKey("line")) {
            if (!effectNode["line"].isNumber()) return "line is not a number";
            targetLine = effectNode["line"].getNumber();
        }

        ep_u64 order = 0;
        if (effectNode.hasKey("order")) {
            if (!effectNode["order"].isNumber()) return "order is not a number";
            order = effectNode["order"].getNumber();
        }

        if (!effectNode.hasKey("shader")) return "missing shader field";
        if (!effectNode["shader"].isString()) return "shader is not a string";
        auto shaderName = effectNode["shader"].getString();

        auto& item = extra.effects.emplace_back();
        item.timeZone = { startTime, endTime };
        item.targetLine = targetLine;
        item.order = order;
        item.isGlobal = isGlobal;
        item.shaderName = shaderName;

        if (effectNode.hasKey("vars")) {
            if (!effectNode["vars"].isObject()) return "vars is not an object";
            auto& varsNode = effectNode["vars"].getObject();

            for (auto& [uniformName, eventsNode] : varsNode) {
                auto& layer = item.uniforms[uniformName];

                if (eventsNode.isArray()) {
                    auto& eventsArr = eventsNode.getArray();
                    if (eventsArr.empty()) return "events array is empty";

                    JsonNode::EnumType eventItemNodeType = eventsArr[0].type;
                    for (auto& node : eventsArr) {
                        if (node.type != eventItemNodeType) return "events array contains different types of nodes";
                    }

                    if (eventItemNodeType == JsonNode::EnumType::Object) {
                        for (auto& eventNode : eventsArr) {
                            if (!eventNode.hasKey("startTime")) return "missing startTime field";
                            if (!eventNode.hasKey("endTime")) return "missing endTime field";

                            ep_f64 startTime, endTime;
                            if (!parseTimeTupleToSecond(eventNode["startTime"], &startTime)) return "startTime is not a valid time tuple";
                            if (!parseTimeTupleToSecond(eventNode["endTime"], &endTime)) return "endTime is not a valid time tuple";

                            if (!eventNode.hasKey("start")) return "missing start field";
                            if (!eventNode.hasKey("end")) return "missing end field";
                            if (eventNode["start"].type != eventNode["end"].type) return "start and end are not the same type";

                            Vec2 valueZone;
                            
                            if (eventNode["start"].isNumber()) {
                                valueZone = assets.requestShaderUniformPair(eventNode["start"].getNumber(), eventNode["end"].getNumber());
                            } else if (eventNode["start"].isArray()) {
                                PhiShaderUniform startUniform, endUniform;
                                if (!parseVectorUniform(eventNode["start"], &startUniform)) return "start is not a valid vector uniform";
                                if (!parseVectorUniform(eventNode["end"], &endUniform)) return "end is not a valid vector uniform";
                                valueZone = assets.requestShaderUniformPair(startUniform, endUniform);
                            } else return "start and end are not a number or array";

                            ep_u64 easingType = 1;
                            if (eventNode.hasKey("easingType")) {
                                if (!eventNode["easingType"].isNumber()) return "easingType is not a number";
                                easingType = eventNode["easingType"].getNumber();
                            }

                            PhiEvent e {};
                            e.timeZone = { startTime, endTime };
                            e.valueZone = valueZone;
                            e.type = EnumPhiEventType::PhiShaderUniform;
                            e.layerIndex = PhiEventLayerIndexs::SHADER_UNIFORM_DEFAULT;

                            if (easingType > 1) {
                                e.easingFuncContext = (void*)easingType;
                                e.easingFunc = [](void* ctx, ep_f64 p) { return EaseSet::Phigros::RePhiEdit::easing((ep_u64)ctx, p); };
                            }

                            layer.addEvent(e);
                        }
                    } else if (eventItemNodeType == JsonNode::EnumType::Number) {
                        PhiShaderUniform uniform;
                        if (!parseVectorUniform(eventsNode, &uniform)) return "events item is not a valid vector uniform";
                        layer.addEvent({
                            .timeZone = INF_TZ,
                            .valueZone = assets.requestShaderUniformPair(uniform, uniform),
                            .type = EnumPhiEventType::PhiShaderUniform,
                            .layerIndex = PhiEventLayerIndexs::SHADER_UNIFORM_DEFAULT
                        });
                    } else return "events array item is not an object or number";
                } else if (eventsNode.isNumber()) {
                    layer.addEvent({
                        .timeZone = INF_TZ,
                        .valueZone = assets.requestShaderUniformPair(eventsNode.getNumber(), eventsNode.getNumber()),
                        .type = EnumPhiEventType::PhiShaderUniform,
                        .layerIndex = PhiEventLayerIndexs::SHADER_UNIFORM_DEFAULT
                    });
                } else return "event(s) is not an array or number";
            }
        }
    }

    return extra;
}

struct PhiStoryboardHelpers {
    /* !docs
    A helper function set for phigros storyboard assets.
    */

    static std::string textureNameToPath(const std::string& dir, const std::string& name) {
        return std::filesystem::path(dir + "/" + name)
            .lexically_normal()
            .string();
    }

    static void attachTextureLoader(
        PhiStoryboardAssets& assets,
        const std::string& dir,
        const std::function<std::optional<std::pair<ep_u64, Vec2>>(std::string)>& loader,
        const std::function<void(ep_u64)>& destroyer
    ) {
        assets.clearTextures();
        assets.textureLoader = [=](std::string name) { return loader(textureNameToPath(dir, name)); };
        assets.textureDestroyer = destroyer;
    }

    static std::unordered_map<std::string, PhiShaderUniform> parseDefaultShaderUniforms(
        const std::string& code
    ) {
        std::vector<std::string> lines;
        splitString(code, lines);

        std::unordered_map<std::string, PhiShaderUniform> result;

        for (auto& line : lines) {
            stripString(line);
            if (!stringIsStartsWith(line, "uniform ")) continue;

            auto s = line.find('%');
            if (s == std::string::npos) continue;

            auto e = line.find('%', s);
            if (e == std::string::npos) continue;

            auto default_str = line.substr(s + 1, e - s - 1);

            std::vector<std::string> value_strs;
            splitString(default_str, value_strs, ',');

            std::vector<ep_f64> values;
            for (auto& value_str : value_strs) {
                stripString(value_str);
                try { values.push_back(std::stod(value_str)); }
                catch (...) { values.push_back(0); }
            }

            if (values.empty() || values.size() > 4) continue;

            s = line.find(';');
            if (s == std::string::npos) continue;
            e = s;
            while (e > 0 && line[e - 1] != ' ') e--;

            result[line.substr(e, s - e)] = PhiShaderUniform(values);
        }

        return result;
    }
};

struct ParsedRPEChartInfo {
    /* !docs
    A struct for parsed RPE chart info.
    */

    std::string name;
    std::string path;
    std::string song;
    std::string picture;
    std::string chart;
    std::string level;
    std::string composer;
    std::string lastEditTime;
    std::string length;
    std::string editTime;
    std::string group;

    static std::vector<ParsedRPEChartInfo> parse(const Data& data) {
        /* !docs
        Parse RPE chart infos from a data object.
        */

        std::vector<ParsedRPEChartInfo> infos;

        auto str = data.toString();
        str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());

        std::vector<std::string> lines;
        splitString(str, lines);

        ParsedRPEChartInfo info {};
        ep_u64 vaildLineCount = 0;

        for (auto& line : lines) {
            stripString(line);

            if (line.empty()) continue;
            if (line[0] == '#') {
                if (vaildLineCount) {
                    infos.push_back(info);
                    info = {};
                    vaildLineCount = 0;
                }
                continue;
            }

            auto split = line.find(": ");
            if (split == std::string::npos) continue;

            auto key = line.substr(0, split);
            auto value = line.substr(split + 2);

            if (key == "Name") info.name = value;
            else if (key == "Path") info.path = value;
            else if (key == "Song") info.song = value;
            else if (key == "Picture") info.picture = value;
            else if (key == "Chart") info.chart = value;
            else if (key == "Level") info.level = value;
            else if (key == "Composer") info.composer = value;
            else if (key == "LastEditTime") info.lastEditTime = value;
            else if (key == "Length") info.length = value;
            else if (key == "EditTime") info.editTime = value;
            else if (key == "Group") info.group = value;
            vaildLineCount++;
        }

        if (vaildLineCount) {
            infos.push_back(info);
        }

        return infos;
    }
};

struct PhiCalculateFrameConfig {
    /* !docs
    Configuration for calculating a frame.
    */

    struct NoteTextureInfo {
        struct Item {
            Vec2 textureSize;
            Vec2 cutPadding; // !inline-docs| It means the padding of the texture when cutting the texture, its unit is pixels.
            Vec2 scaling = { 1.0, 1.0 };
        };

        Item single;
        Item simul;
    };

    Vec2 screenSize;
    Vec2 backgroundTextureSize; // !inline-docs| The real size of the background texture.
    std::unordered_map<EnumPhiNoteType, NoteTextureInfo> noteTextureInfos;
    ep_f64 songLength; // !inline-docs| In seconds.
    ep_f64 maxNoteBodyLength = 8192.0;
    ep_f64 maxFontSizeNormScale = 16.0;
};

struct PhiCalculatedFrame {
    /* !docs
    The calculated frame.
    */

    using CalculatedText = SharedCalculatedObjects::CalculatedText;
    using CalculatedRect = SharedCalculatedObjects::CalculatedRect;
    using CalculatedPoly = SharedCalculatedObjects::CalculatedPoly;

    struct CalculatedNote {
        Vec2 position;
        ep_f64 rotation;
        ep_f64 width, head, body, tail;
        EnumPhiNoteType type;
        bool isSimul;
        Color color;
    };

    struct CalculatedStoryboardTexture {
        ep_u64 texture;
        Vec2 position, size, scale, anchor;
        ep_f64 rotation;
        Color color;
    };

    struct CalculatedHitEffectTexture {
        Vec2 position, size;
        ep_f64 progress;
        ep_f64 rotation;
        Color color;
    };

    struct CalculatedShader {
        ep_u64 id;
        std::unordered_map<std::string, PhiShaderUniform> uniforms;
    };

    using CalculatedObject = std::variant<
        CalculatedText,
        CalculatedRect,
        CalculatedPoly,
        CalculatedNote,
        CalculatedStoryboardTexture,
        CalculatedHitEffectTexture,
        CalculatedShader
    >;

    ep_f64 backgroundImageBlurRadius;
    Rect unsafeBackgroundRect, backgroundRect;
    ep_f64 unsafeAreaDim, backgroundDim;
    Rect objectsClipRect;
    std::vector<CalculatedObject> objects;
    std::vector<std::pair<EnumPhiNoteType, ep_f64>> hitsounds;

    struct Cache {
        struct AttachUIData {
            /* !docs
            Data for attaching a phigros line which is attached ui.
            */

            Vec2 position, scale = { 1.0, 1.0 };
            ep_f64 rotation;
            Color color = { 1.0, 1.0, 1.0, 1.0 };
        };

        std::unordered_map<EnumPhiLineAttachUI, AttachUIData> attachUIDatas;
        std::unordered_map<EnumPhiNoteType, std::vector<CalculatedObject>> noteObjects;

        void clear() {
            attachUIDatas.clear();
            for (auto& [_, objects] : noteObjects) objects.clear();
        }
    };

    Cache cache;
    Vec2 frameTimeRange;
};

void calculatePhiFrame(
    PhiChart& chart, ep_f64 time,
    const PhiCalculateFrameConfig& config,
    PhiCalculatedFrame& frame
) {
    /* !docs
    Calculate a frame of the chart at the given time.
    */

    frame.objects.clear();
    frame.hitsounds.clear();
    frame.cache.clear();

    frame.frameTimeRange = { frame.frameTimeRange.y, time };

    ep_f64 screenRatio = config.screenSize.x / config.screenSize.y;
    Rect safeArea = screenRatio > chart.meta.maxViewRatio ? getCoveredOrContainRect(
        { 0.0, 0.0, config.screenSize.x, config.screenSize.y },
        { chart.meta.maxViewRatio, 1.0 }, false
    ) : Rect { 0.0, 0.0, config.screenSize.x, config.screenSize.y };

    auto safeAreaPosition = safeArea.position();
    auto safeAreaSize = safeArea.size();
    auto toScreen = [&](Vec2 pos) { return pos + safeAreaPosition; };

    frame.backgroundImageBlurRadius = config.backgroundTextureSize.sum() * chart.options.backgroundTextureBlurRadius;

    frame.unsafeBackgroundRect = getCoveredOrContainRect(
        { 0.0, 0.0, config.screenSize.x, config.screenSize.y },
        config.backgroundTextureSize, true
    );
    frame.unsafeAreaDim = chart.options.unsafeBackgroundDim;

    frame.backgroundRect = getCoveredOrContainRect(safeArea, config.backgroundTextureSize, true);
    frame.backgroundDim = chart.options.backgroundDim;

    frame.objectsClipRect = safeArea;

    auto processAttachUIText = [&](PhiCalculatedFrame::CalculatedText rawText, EnumPhiLineAttachUI attachUIType) {
        auto& data = frame.cache.attachUIDatas[attachUIType];
        rawText.position += data.position;
        rawText.scale *= data.scale;
        rawText.rotation += data.rotation;
        rawText.color *= data.color;
        return rawText;
    };

    time -= chart.meta.offset;
    
    auto lineWidth = (chart.meta.lineWidthUnit * safeAreaSize).sum();
    auto lineHeight = (chart.meta.lineHeightUnit * safeAreaSize).sum();
    auto standardNoteWidth = safeAreaSize.x * 0.1234375 * chart.options.noteScaling;

    struct NoteTextureSizeInfo {
        ep_f64 width;
        ep_f64 head, body, tail;

        void scale(const Vec2& v) {
            width *= v.x;
            head *= v.y;
            body *= v.y;
            tail *= v.y;
        }

        ep_f64 getHeadHalfDiagonal() const {
            return Vec2 { width, head }.length() / 2;
        }
    };

    auto getNoteTextureSizeInfo = [&](EnumPhiNoteType type, bool isSimul, bool hideHead) {
        const auto& texInfo = (
            isSimul
            ? config.noteTextureInfos.at(type).simul
            : config.noteTextureInfos.at(type).single
        );

        auto width = standardNoteWidth;
        auto totalHeight = width / texInfo.textureSize.x * texInfo.textureSize.y;
        width *= texInfo.scaling.x; totalHeight *= texInfo.scaling.y;
        auto cutPadding = texInfo.cutPadding / texInfo.textureSize.y;
        auto head = hideHead ? 0.0 : cutPadding.x * totalHeight;
        auto tail = cutPadding.y * totalHeight;

        return NoteTextureSizeInfo {
            .width = width,
            .head = head,
            .tail = tail
        };
    };

    ep_f64 maxHalfNoteHeadDiagonal = 0.0;

    for (auto& [type, _] : config.noteTextureInfos) {
        maxHalfNoteHeadDiagonal = std::max({
            maxHalfNoteHeadDiagonal,
            getNoteTextureSizeInfo(type, false, false).getHeadHalfDiagonal(),
            getNoteTextureSizeInfo(type, true, false).getHeadHalfDiagonal()
        });
    }

    chart.state.timeUpdated(time);

    for (auto& lineIndex : chart.zOrderSortedLines) {
        auto& line = chart.lines[lineIndex];

        auto linePosition = chart.getLinePosition(time, line, safeAreaSize);
        auto lineScreenPosition = toScreen(linePosition);
        auto linePositionRelOrigin = chart.getLinePositionRelOrigin(time, line, safeAreaSize);
        auto lineRotation = chart.animator.get(line, time, EnumPhiEventType::SelfRotation);
        auto lineAlpha = chart.animator.get_alpha(line, time, 0.0);
        auto lineTextIndex = chart.animator.get(line, time, EnumPhiEventType::Text);
        auto lineTextIndexZone = chart.animator.get_zone(line, time, EnumPhiEventType::Text);
        auto lineText = chart.storyboardAssets.getText(lineTextIndex, lineTextIndexZone);
        auto lineColorIndex = chart.animator.get(line, time, EnumPhiEventType::Color);
        auto lineColorIndexZone = chart.animator.get_zone(line, time, EnumPhiEventType::Color);
        auto lineColor = chart.storyboardAssets.getColor(
            lineColorIndex,
            (line.attachUI.has_value() || lineText.has_value() || line.textureName.has_value())
                ? Color::White()
                : chart.options.lineDefaultColor,
            lineColorIndexZone
        );
        auto lineScale = Vec2 {
            chart.animator.get(line, time, EnumPhiEventType::ScaleX),
            chart.animator.get(line, time, EnumPhiEventType::ScaleY)
        };

        if (line.attachUI.has_value()) {
            frame.cache.attachUIDatas[line.attachUI.value()] = {
                .position = linePositionRelOrigin,
                .scale = lineScale,
                .rotation = lineRotation,
                .color = lineColor.applyAlpha(lineAlpha)
            };
        }

        if (lineAlpha * lineColor.a > 0) {
            if (lineText.has_value()) {
                frame.objects.push_back(PhiCalculatedFrame::CalculatedText {
                    .text = lineText.value(),
                    .position = lineScreenPosition,
                    .scale = lineScale,
                    .anchor = line.anchor,
                    .fontSize = (chart.options.storyboardTextBaseSize * safeAreaSize).sum(),
                    .rotation = lineRotation,
                    .color = lineColor.applyAlpha(lineAlpha)
                });
            } else if (!line.attachUI.has_value()) {
                if (line.textureName.has_value()) {
                    auto& textureName = line.textureName.value();

                    if (chart.storyboardAssets.isTextureLoaded(textureName)) {
                        auto& texture = chart.storyboardAssets.getTexture(textureName);
                        ep_f64 textureWidth, textureHeight;

                        if (chart.options.storyboardTextureSclaingBehavior == PhiChart::UserOptions::EnumStoryboardTextureSclaingBehavior::AboutWidth) {
                            textureWidth = texture.second.x / std::abs(chart.meta.worldViewport.x) * safeAreaSize.x;
                            textureHeight = textureWidth / texture.second.x * texture.second.y;
                        } else if (chart.options.storyboardTextureSclaingBehavior == PhiChart::UserOptions::EnumStoryboardTextureSclaingBehavior::AboutHeight) {
                            textureHeight = texture.second.y / std::abs(chart.meta.worldViewport.y) * safeAreaSize.y;
                            textureWidth = textureHeight / texture.second.y * texture.second.x;
                        } else if (chart.options.storyboardTextureSclaingBehavior == PhiChart::UserOptions::EnumStoryboardTextureSclaingBehavior::Stretch) {
                            textureWidth = texture.second.x / std::abs(chart.meta.worldViewport.x) * safeAreaSize.x;
                            textureHeight = texture.second.y / std::abs(chart.meta.worldViewport.y) * safeAreaSize.y;
                        } else textureWidth = textureHeight = 0;

                        textureWidth *= chart.options.storyboardTextureScaling.x;
                        textureHeight *= chart.options.storyboardTextureScaling.y;

                        frame.objects.push_back(PhiCalculatedFrame::CalculatedStoryboardTexture {
                            .texture = texture.first,
                            .position = lineScreenPosition,
                            .size = Vec2 { textureWidth, textureHeight },
                            .scale = lineScale,
                            .anchor = line.anchor,
                            .rotation = lineRotation,
                            .color = lineColor.applyAlpha(lineAlpha)
                        });
                    }
                } else {
                    frame.objects.push_back(PhiCalculatedFrame::CalculatedPoly::Make(
                        Vec2 { -lineWidth, -lineHeight } * line.anchor * lineScale,
                        Vec2 { lineWidth, lineHeight } * lineScale,
                        lineColor.applyAlpha(lineAlpha),
                        Transform2D()
                            .translate(lineScreenPosition)
                            .rotateDegrees(lineRotation)
                    ));
                }
            }
        }

        for (auto& noteGroup : line.noteGroups) {
            noteGroup.state.timeUpdated(time);

            for (ep_u64 note_ii = noteGroup.state.firstNoteIndex; note_ii < noteGroup.indexs.size(); note_ii++) {
                auto note_i = noteGroup.indexs[note_ii];
                auto& note = line.notes[note_i];
                note.state.timeUpdated(time);

                auto frameInfo = chart.getNoteFrameInfo(line, note, time, safeAreaSize);

                if (frameInfo.isArrived && note.state.onPlayHitsound()) {
                    if (!note.isFake) {
                        frame.hitsounds.push_back({ note.type, time - note.time });
                    }
                }

                if (note.time + note.holdTime < time) {
                    noteGroup.state.passedNoteIndex(note_ii);
                    continue;
                }

                auto noteScreenHeadPosition = toScreen(frameInfo.headPosition);
                auto noteScreenTailPosition = toScreen(frameInfo.tailPosition);

                auto sizeInfo = getNoteTextureSizeInfo(note.type, note.isSimul, frameInfo.isArrived);
                sizeInfo.body = std::min(config.maxNoteBodyLength, (noteScreenHeadPosition - noteScreenTailPosition).length());
                sizeInfo.scale(frameInfo.scale);

                Transform2D noteTransform;
                noteTransform.translate(noteScreenHeadPosition);
                noteTransform.rotateDegrees(frameInfo.textureRotation);
                noteTransform.scale(1.0, -1.0);

                Vec2 noteQuad[4] = {
                    noteTransform.transformPoint({ -sizeInfo.width / 2, -sizeInfo.head }),
                    noteTransform.transformPoint({ sizeInfo.width / 2, -sizeInfo.head }),
                    noteTransform.transformPoint({ sizeInfo.width / 2, sizeInfo.body + sizeInfo.tail }),
                    noteTransform.transformPoint({ -sizeInfo.width / 2, sizeInfo.body + sizeInfo.tail })
                };

                // 只 hide 不用考虑 maxHalfNoteHeadDiagonal, 但是这里 break 优化也要用
                auto extendedSafeArea = safeArea.extend(maxHalfNoteHeadDiagonal);
                bool noteInsideScreen = quadStrictlyIntersectRect(noteQuad, extendedSafeArea);

                if (noteInsideScreen) {
                    if (frameInfo.isVisible) {
                        frame.cache.noteObjects[note.type].push_back(PhiCalculatedFrame::CalculatedNote {
                            .position = noteScreenHeadPosition,
                            .rotation = frameInfo.textureRotation,
                            .width = sizeInfo.width,
                            .head = sizeInfo.head,
                            .body = sizeInfo.body,
                            .tail = sizeInfo.tail,
                            .type = note.type,
                            .isSimul = note.isSimul,
                            .color = frameInfo.color
                        });
                    }
                } else {
                    if (chart.options.enableNoteOffScreenBreakOptimization && noteGroup.breakable) {
                        if (lineIsLeavingScreen(
                            noteScreenHeadPosition,
                            frameInfo.speedVectorRotation,
                            extendedSafeArea
                        ) && lineIsLeavingScreen(
                            noteScreenHeadPosition,
                            frameInfo.textureRotation,
                            extendedSafeArea
                        )) break;
                    }
                }
            }
        }
    }

    for (const auto type : {
        EnumPhiNoteType::Hold,
        EnumPhiNoteType::Drag,
        EnumPhiNoteType::Tap,
        EnumPhiNoteType::Flick
    }) {
        auto& noteObjects = frame.cache.noteObjects[type];
        frame.objects.insert(frame.objects.end(), noteObjects.begin(), noteObjects.end());
    }

    const ep_f64 hitEffectTextureSize = standardNoteWidth * chart.options.hitEffectTextureScaling;

    for (ep_u64 i = chart.state.firstHitEffectIndex; i < chart.hitEffects.size(); i++) {
        auto& hitEffect = chart.hitEffects[i];
        if (hitEffect.time > time) break;

        auto& line = chart.lines[hitEffect.lineIndex];
        auto& note = line.notes[hitEffect.noteIndex];

        auto info = chart.getNoteFrameInfo(line, note, hitEffect.time, safeAreaSize);
        auto endTime = hitEffect.time + std::max(chart.options.hitEffectDuration, hitEffect.particles.size() ? (hitEffect.particles.back().dt + chart.options.hitEffectDuration) : 0.0);

        if (endTime < time) {
            chart.state.passedHitEffectIndex(i);
            continue;
        }

        auto effectScreenPosition = toScreen(info.headPosition);
        auto progress = (time - hitEffect.time) / chart.options.hitEffectDuration;

        if (progress <= 1.0) {
            frame.objects.push_back(PhiCalculatedFrame::CalculatedHitEffectTexture {
                .position = effectScreenPosition,
                .size = { hitEffectTextureSize, hitEffectTextureSize },
                .progress = progress,
                .rotation = 0.0,
                .color = chart.options.lineDefaultColor.applyAlpha(chart.options.hitEffectAlpha)
            });
        }

        for (auto& particle : hitEffect.particles) {
            auto particleTime = hitEffect.time + particle.dt;
            if (particleTime > time) break;
            if (particleTime + chart.options.hitEffectDuration < time) continue;

            auto info = chart.getNoteFrameInfo(line, note, particleTime, safeAreaSize);
            auto effectScreenHeadPosition = toScreen(info.headPosition);
            auto progress = std::clamp((time - particleTime) / chart.options.hitEffectDuration, 0.0, 1.0);
            auto size = standardNoteWidth / 5.3 * chart.options.hitEffectParticleSize * (((0.20783014 * progress - 1.65243926) * progress + 1.6398785) * progress + 0.49884492);
            auto distance = standardNoteWidth / 180 * chart.options.hitEffectParticleDistance * particle.size * (((850.3997391752 * progress + 6236.3848902154) * progress + 80.3542231806) * progress / ((6570.5817658876 * progress + 495.7977913926) * progress + 1.0));

            auto particlePosition = toScreen(info.headPosition.rotateDegrees(particle.rotation, distance));
            frame.objects.push_back(PhiCalculatedFrame::CalculatedRect {
                .position = particlePosition,
                .size = { size, size },
                .rotation = 0.0,
                .color = chart.options.lineDefaultColor.applyAlpha(chart.options.hitEffectAlpha * (1.0 - progress))
            });
        }
    }

    auto calculateExtra = [&](bool isGlobal) {
        for (auto& effectIndex : chart.extra.zOrderSortedEffects) {
            auto& effect = chart.extra.effects[effectIndex];
            if (effect.isGlobal != isGlobal) continue;
            if (!effect.timeZone.include(time)) continue;

            PhiCalculatedFrame::CalculatedShader shader { .id = effect.shaderId };

            for (auto& [uniformName, layer] : effect.uniforms) {
                layer.updateType(EnumPhiEventType::PhiShaderUniform, time);
                auto uniformIndex = layer.get(EnumPhiEventType::PhiShaderUniform);
                auto uniformIndexZone = layer.get_zone(EnumPhiEventType::PhiShaderUniform).value_or({});
                auto uniformValue = chart.storyboardAssets.getShaderUniform(uniformIndex, PhiShaderUniform(), uniformIndexZone);
                shader.uniforms[uniformName] = uniformValue;
            }

            frame.objects.push_back(shader);
        }
    };

    calculateExtra(false);

    auto combo = chart.getCombo(time);

    time += chart.meta.offset;
    ep_f64 songPorgress = time / config.songLength;

    ep_f64 progressBarHeight = safeAreaSize.x * 0.005921;
    ep_f64 progressBarWidth = safeAreaSize.x * songPorgress;
    ep_f64 progressBarPointWidth = safeAreaSize.x * 0.00175;

    auto& progressBarAttachUIData = frame.cache.attachUIDatas[EnumPhiLineAttachUI::Bar];

    frame.objects.push_back(PhiCalculatedFrame::CalculatedPoly::Make(
        { 0.0, 0.0 },
        { progressBarWidth, progressBarHeight },
        chart.options.progressBarDefaultColor.first * progressBarAttachUIData.color,
        Transform2D()
            .translate(safeAreaPosition)
            .translate(progressBarAttachUIData.position)
            .scale(progressBarAttachUIData.scale)
            .rotateDegrees(progressBarAttachUIData.rotation)
    ));

    frame.objects.push_back(PhiCalculatedFrame::CalculatedPoly::Make(
        { progressBarWidth - progressBarPointWidth, 0.0 },
        { progressBarPointWidth, progressBarHeight },
        chart.options.progressBarDefaultColor.second * progressBarAttachUIData.color,
        Transform2D()
            .translate(safeAreaPosition)
            .translate(progressBarAttachUIData.position)
            .scale(progressBarAttachUIData.scale)
            .rotateDegrees(progressBarAttachUIData.rotation)
    ));

    auto pauseButtonPosition = Vec2 { 3.16669, 3.6065 } * progressBarHeight;
    auto pauseButtonSize = Vec2 { safeAreaSize.x * 32 / 1920, safeAreaSize.x * 37.48 / 1920 };
    ep_f64 pauseButtonItemWidth = pauseButtonSize.x * 0.323;

    auto& pauseButtonAttachUIData = frame.cache.attachUIDatas[EnumPhiLineAttachUI::Pause];

    frame.objects.push_back(PhiCalculatedFrame::CalculatedPoly::Make(
        { 0.0, 0.0 },
        { pauseButtonItemWidth, pauseButtonSize.y },
        pauseButtonAttachUIData.color,
        Transform2D()
            .translate(safeAreaPosition)
            .translate(pauseButtonPosition)
            .translate(pauseButtonAttachUIData.position)
            .scale(pauseButtonAttachUIData.scale)
            .rotateDegrees(pauseButtonAttachUIData.rotation)
    ));

    frame.objects.push_back(PhiCalculatedFrame::CalculatedPoly::Make(
        { pauseButtonSize.x - pauseButtonItemWidth, 0.0 },
        { pauseButtonItemWidth, pauseButtonSize.y },
        pauseButtonAttachUIData.color,
        Transform2D()
            .translate(safeAreaPosition)
            .translate(pauseButtonPosition)
            .translate(pauseButtonAttachUIData.position)
            .scale(pauseButtonAttachUIData.scale)
            .rotateDegrees(pauseButtonAttachUIData.rotation)
    ));

    if (combo >= 3) {
        frame.objects.push_back(processAttachUIText(PhiCalculatedFrame::CalculatedText {
            .text = std::to_string(combo),
            .position = toScreen({ safeAreaSize.x / 2, safeAreaSize.x * 0.027083 }),
            .scale = { 1.0, 1.0 },
            .anchor = { 0.5, 0.5 },
            .fontSize = safeAreaSize.x * 0.0393081,
            .rotation = 0.0,
            .color = Color::White()
        }, EnumPhiLineAttachUI::ComboNumber));

        frame.objects.push_back(processAttachUIText(PhiCalculatedFrame::CalculatedText {
            .text = "AUTOPLAY",
            .position = toScreen({ safeAreaSize.x / 2, safeAreaSize.x * 0.0478125 }),
            .scale = { 1.0, 1.0 },
            .anchor = { 0.5, 0.0 },
            .fontSize = safeAreaSize.x * 0.0130208,
            .rotation = 0.0,
            .color = Color::White()
        }, EnumPhiLineAttachUI::Combo));
    }

    ep_u64 score = chart.comboTimes.size() ? std::clamp<ep_f64>(std::ceil((ep_f64)1000000 / chart.comboTimes.size() * combo), 0, 1000000) : 1000000;
    frame.objects.push_back(processAttachUIText(PhiCalculatedFrame::CalculatedText {
        .text = formatToStdString("%07llu", score),
        .position = toScreen({ safeAreaSize.x * (1 - ((ep_f64)40 / 1920)), safeAreaSize.x * 0.01614583 }),
        .scale = { 1.0, 1.0 },
        .anchor = { 1.0, 0.0 },
        .fontSize = safeAreaSize.x * 0.0277778,
        .rotation = 0.0,
        .color = Color::White()
    }, EnumPhiLineAttachUI::Score));
    
    frame.objects.push_back(processAttachUIText(PhiCalculatedFrame::CalculatedText {
        .text = chart.meta.title,
        .position = toScreen({ safeAreaSize.x * 0.0225, safeAreaSize.y - safeAreaSize.x * 0.0196875 }),
        .scale = { 1.0, 1.0 },
        .anchor = { 0.0, 1.0 },
        .fontSize = safeAreaSize.x * 0.018115942,
        .rotation = 0.0,
        .color = Color::White()
    }, EnumPhiLineAttachUI::Name));
    
    frame.objects.push_back(processAttachUIText(PhiCalculatedFrame::CalculatedText {
        .text = chart.meta.difficulty,
        .position = toScreen({ safeAreaSize.x * 0.9775, safeAreaSize.y - safeAreaSize.x * 0.0196875 }),
        .scale = { 1.0, 1.0 },
        .anchor = { 1.0, 1.0 },
        .fontSize = safeAreaSize.x * 0.018115942,
        .rotation = 0.0,
        .color = Color::White()
    }, EnumPhiLineAttachUI::Level));

    calculateExtra(true);
}

struct PhiTakeOverer {
    /* !docs
    The take overer for phigros.

    Following functions are necessary to be set:

    - noteTextureDataLoader
    - hitEffectDataLoader
    - hitsoundDataLoader
    - storyboardDataLoader
    - shaderDataLoader
    - glCtx
    - sharedComp.textureDecoder
    - textManager.renderer
    - audioManager.decoder
    - audioManager.engine
    */

    PhiTakeOverer() = default;
    PhiTakeOverer(const PhiTakeOverer&) = delete;
    PhiTakeOverer(PhiTakeOverer&&) = delete;
    PhiTakeOverer& operator=(const PhiTakeOverer&) = delete;
    PhiTakeOverer& operator=(PhiTakeOverer&&) = delete;

    static ep_sp<PhiTakeOverer> Make() {
        auto* tor = new PhiTakeOverer();
        return ep_sp<PhiTakeOverer>(tor);
    }

    struct NoteTextureDataLoaderConfig {
        EnumPhiNoteType type;
        bool isSimul;
    };

    struct NoteTextureDataLoaderResult {
        Data encoded;
        Vec2 cutPadding;
        bool cutPaddingIsPixel = true;
        bool ignoreCutPadding;
    };

    using NoteTextureDataLoader = std::function<NoteTextureDataLoaderResult(const NoteTextureDataLoaderConfig&)>;
    NoteTextureDataLoader noteTextureDataLoader;

    using HitEffectDataLoader = std::function<std::vector<Data>()>;
    HitEffectDataLoader hitEffectDataLoader;

    using HitsoundDataLoader = std::function<Data(EnumPhiNoteType)>;
    HitsoundDataLoader hitsoundDataLoader;

    using StoryboardDataLoader = std::function<Data(const std::string&)>;
    StoryboardDataLoader storyboardDataLoader;

    using ShaderDataLoader = std::function<std::string(const std::string&)>;
    ShaderDataLoader shaderDataLoader;

    ep_sp<GL::GL33Context> glCtx;
    TakeOvererComponents::SharedComp sharedComp;
    GL::TextManager textManager;
    TakeOvererComponents::AudioManager audioManager;

    PhiCalculateFrameConfig calcConfig;
    PhiChart chart;
    PhiCalculatedFrame calculatedFrame;

    void init() {
        checkBoolAndThrow(!!noteTextureDataLoader, "noteTextureDataLoader is not set");
        checkBoolAndThrow(!!hitEffectDataLoader, "hitEffectDataLoader is not set");
        checkBoolAndThrow(!!hitsoundDataLoader, "hitsoundDataLoader is not set");
        checkBoolAndThrow(!!storyboardDataLoader, "storyboardDataLoader is not set");
        checkBoolAndThrow(!!shaderDataLoader, "shaderDataLoader is not set");
        checkBoolAndThrow(!!glCtx, "glCtx is not set");

        textManager.glCtx = glCtx;

        sharedComp.check();
        textManager.check();
        audioManager.check();

        loadResources();
    }

    void loadIllustion(const Data& data) {
        auto decoded = sharedComp.textureDecoder(data);
        sharedComp.illustionTexture = glCtx->createTextureFromDecoded(decoded);
        bluredIllustionCache.key = -1.0;
    }

    void loadIllustion(const std::string& path) { loadIllustion(Data::MakeFromFile(path)); }

    struct MixBgmConfig {
        ep_f64 musicVol = 1.0, sfxVol = 1.0;
        bool sfxRandshake = false;
    };

    ep_sp<DecodedAudio> mixFinalBgm(const PhiChart& chart, const MixBgmConfig& config) {
        if (!audioManager.bgmAudio) throw std::runtime_error("bgm is not loaded");

        auto result = audioManager.bgmAudio->copy();
        result->applyVolume(config.musicVol);
        
        std::mt19937 rng { std::random_device {} () };
        std::uniform_real_distribution<double> sfxRandshakeDist { 0.0, 0.02 };

        for (const auto& line : chart.lines) {
            for (const auto& note : line.notes) {
                if (note.isFake) continue;

                ep_f64 t = note.time + chart.meta.offset;
                if (config.sfxRandshake) t += sfxRandshakeDist(rng);

                auto sfx = hitsoundAudios.at(note.type);
                result->overlapSecond(sfx, t, config.sfxVol);
            }
        }

        return result;
    }

    using ChartIniter = std::function<void(PhiChart&)>;

    TakeOvererComponents::LoadChartResultInfo loadChart(
        const Data& data,
        const ChartIniter& initer = [](PhiChart& chart) { chart.init(); }
    ) {
        TakeOvererComponents::LoadChartResultInfo resultInfo {};

        {
            Timer timer;
            auto loadResult = loadPhiChartFromData(data);
            resultInfo.createObjectTook = timer.elapsed();

            if (!loadResult.success) {
                resultInfo.success = false;
                resultInfo.errors = std::move(loadResult.errors);
                return resultInfo;
            }

            chart = std::move(loadResult.chart);
        }

        ep_u64 storyboardTextureId = 0;

        chart.storyboardAssets.clearTextures();

        chart.storyboardAssets.textureLoader = [&, this](const std::string& name) {
            auto data = storyboardDataLoader(name);
            auto decoded = sharedComp.textureDecoder(data);
            auto tex = glCtx->createTextureFromDecoded(decoded);
            auto id = storyboardTextureId++;
            storyboardTextures[id] = tex;
            return std::make_pair(id, Vec2 { (ep_f64)decoded.width, (ep_f64)decoded.height });
        };

        chart.storyboardAssets.textureDestroyer = [this](ep_u64 id) {
            storyboardTextures.erase(id);
        };

        chart.storyboardAssets.shaderPreloader = [this](const std::string& name, ep_u64 id) {
            auto shaderString = shaderDataLoader(name);

            if (shaderString.empty()) {
                throw std::runtime_error("shader string is empty: " + name);
            }

            try {
                auto prog = glCtx->createConfiguredProgram(shaderString, GL::defaultVertexShaderSource_RPE);
                prog->fragConfig.textureUniformName = "screenTexture";
                prog->fragConfig.colorUniformName = std::nullopt;
                shaders[id] = prog;
            } catch (const std::exception& e) {
                throw std::runtime_error("failed to load shader: " + name + "\n" + e.what());
            }

            auto defaultUnfiroms = PhiStoryboardHelpers::parseDefaultShaderUniforms(shaderString);
            shadersDefaultUniforms[id] = defaultUnfiroms;
        };

        shaders.clear();
        shadersDefaultUniforms.clear();

        {
            Timer timer;
            initer(chart);
            resultInfo.initTook = timer.elapsed();
        }

        return resultInfo;
    }

    struct RenderConfig {
        TakeOvererComponents::RenderConfigBase base;
    };

    struct RenderResultInfo {
        TakeOvererComponents::RenderResultInfoBase base;
    };

    RenderResultInfo& render(const RenderConfig& renderConfig) {
        calcConfig.songLength = audioManager.getBgmLength();
        calcConfig.backgroundTextureSize = { sharedComp.illustionTexture->width, sharedComp.illustionTexture->height };

        auto t = renderConfig.base.getTime(audioManager);

        {
            Timer timer;
            calculatePhiFrame(chart, t, calcConfig, calculatedFrame);
            renderResultInfoCache.base.calculatedTook = timer.elapsed();
        }

        Timer glOpsTimer;

        using namespace GL;

        glCtx->setViewport(calcConfig.screenSize.x, calcConfig.screenSize.y);
        glCtx->setClearColor(0.0, 0.0, 0.0, 0.0);
        glCtx->clear(GL_COLOR_BUFFER_BIT);

        auto illuTex = bluredIllustionCache.get(calculatedFrame.backgroundImageBlurRadius, [&]() {
            auto tex = glCtx->createTexture();
            glCtx->copyTexture(sharedComp.illustionTexture.get(), tex.get());
            glCtx->gaussianBlurToTexture(tex.get(), calculatedFrame.backgroundImageBlurRadius);
            return tex;
        });

        auto cvs = GL33Canvas::Make(glCtx.get());

        cvs.drawRect({
            .position = { calculatedFrame.unsafeBackgroundRect.x, calculatedFrame.unsafeBackgroundRect.y },
            .size = { calculatedFrame.unsafeBackgroundRect.w, calculatedFrame.unsafeBackgroundRect.h },
            .color = GLvec4::Gray(1.0 - calculatedFrame.unsafeAreaDim),
            .texture = illuTex.get()
        });

        glCtx->setViewport(
            calculatedFrame.objectsClipRect.x, calculatedFrame.objectsClipRect.y,
            calculatedFrame.objectsClipRect.w, calculatedFrame.objectsClipRect.h
        );

        cvs.drawRect({
            .position = { calculatedFrame.backgroundRect.x, calculatedFrame.backgroundRect.y },
            .size = { calculatedFrame.backgroundRect.w, calculatedFrame.backgroundRect.h },
            .color = GLvec4::Gray(1.0 - calculatedFrame.backgroundDim),
            .texture = illuTex.get()
        });

        for (auto& obj : calculatedFrame.objects) {
            if (TakeOvererComponents::renderSharedObject(obj, glCtx, cvs, textManager)) {
                continue;
            }
                
            if (std::holds_alternative<PhiCalculatedFrame::CalculatedNote>(obj)) {
                auto& note = std::get<PhiCalculatedFrame::CalculatedNote>(obj);
                auto& img = note.isSimul ? noteTextures[note.type].second : noteTextures[note.type].first;
                auto& imgInfo = note.isSimul ? calcConfig.noteTextureInfos[note.type].simul : calcConfig.noteTextureInfos[note.type].single;

                cvs.save();
                cvs.translate(note.position);
                cvs.rotateDegrees(note.rotation);

                auto mesh = glCtx->requestMesh(6 * 3);
                mesh.color = note.color;
                mesh.texture = img.get();

                mesh.addRect(
                    { -note.width / 2, 0.0 }, { note.width, note.head },
                    GLvec2 { 0.0, imgInfo.textureSize.y - imgInfo.cutPadding.x } / imgInfo.textureSize,
                    GLvec2 { imgInfo.textureSize.x, imgInfo.cutPadding.x } / imgInfo.textureSize
                );

                mesh.addRect(
                    { -note.width / 2, -note.body },
                    { note.width, note.body },
                    GLvec2 { 0.0, imgInfo.cutPadding.y } / imgInfo.textureSize,
                    GLvec2 { imgInfo.textureSize.x, imgInfo.textureSize.y - imgInfo.cutPadding.sum() } / imgInfo.textureSize
                );

                mesh.addRect(
                    { -note.width / 2, -note.body - note.tail },
                    { note.width, note.tail },
                    GLvec2 { 0.0, 0.0 },
                    GLvec2 { imgInfo.textureSize.x, imgInfo.cutPadding.y } / imgInfo.textureSize
                );

                cvs.drawMesh(mesh);
                cvs.restore();
            } else if (std::holds_alternative<PhiCalculatedFrame::CalculatedStoryboardTexture>(obj)) {
                auto& sbTexture = std::get<PhiCalculatedFrame::CalculatedStoryboardTexture>(obj);
                auto& img = storyboardTextures[sbTexture.texture];

                cvs.save();
                cvs.translate(sbTexture.position);
                cvs.rotateDegrees(sbTexture.rotation);
                cvs.scale(sbTexture.scale);
                cvs.drawRect({
                    .position = -sbTexture.size * sbTexture.anchor,
                    .size = sbTexture.size,
                    .color = sbTexture.color,
                    .texture = img.get()
                });
                cvs.restore();
            } else if (std::holds_alternative<PhiCalculatedFrame::CalculatedHitEffectTexture>(obj)) {
                auto& effect = std::get<PhiCalculatedFrame::CalculatedHitEffectTexture>(obj);
                auto& img = hitEffectTextures[std::clamp<ep_u64>(effect.progress * hitEffectTextures.size(), 0, hitEffectTextures.size() - 1)];

                cvs.save();
                cvs.translate(effect.position);
                cvs.rotateDegrees(effect.rotation);
                cvs.drawRect({
                    .position = -effect.size / 2,
                    .size = effect.size,
                    .color = effect.color,
                    .texture = img.get()
                });
                cvs.restore();
            } else if (std::holds_alternative<PhiCalculatedFrame::CalculatedShader>(obj)) {
                auto& shader = std::get<PhiCalculatedFrame::CalculatedShader>(obj);
                auto& prog = shaders[shader.id];
                if (!prog) continue;

                {
                    auto guard = prog->use();

                    for (auto& [k, v] : shadersDefaultUniforms[shader.id]) v.setToGlLocation(prog->getUniformLocation(k));
                    for (auto& [k, v] : shader.uniforms) v.setToGlLocation(prog->getUniformLocation(k));

                    prog->getUniformLocation("screenSize").setf(calcConfig.screenSize.x, calcConfig.screenSize.y);
                }

                auto mesh = glCtx->requestMesh(6);
                mesh.program = prog.get();
                mesh.color = GLvec4::White();
                glCtx->renderToDrawFbo(calcConfig.screenSize.x, calcConfig.screenSize.y, mesh);
            }
        }

        if (renderConfig.base.flushGl) {
            glCtx->flush();
        }

        renderResultInfoCache.base.glOperationsTook = glOpsTimer.elapsed();

        if (!renderConfig.base.disableHitsound) {
            for (ep_u64 i = std::max<ep_i64>(0, calculatedFrame.hitsounds.size() - audioManager.maxSfxPlaying); i < calculatedFrame.hitsounds.size(); ++i) {
                audioManager.playSfx(hitsoundAudios.at(calculatedFrame.hitsounds[i].first));
            }
        }

        return renderResultInfoCache;
    }

    private:
    SKVCache<ep_f64, ep_sp<GL::TextureInfo>> bluredIllustionCache;
    std::unordered_map<EnumPhiNoteType, std::pair<ep_sp<GL::TextureInfo>, ep_sp<GL::TextureInfo>>> noteTextures;
    std::vector<ep_sp<GL::TextureInfo>> hitEffectTextures;
    std::unordered_map<EnumPhiNoteType, ep_sp<DecodedAudio>> hitsoundAudios;
    std::unordered_map<ep_u64, ep_sp<GL::TextureInfo>> storyboardTextures;
    RenderResultInfo renderResultInfoCache;
    std::unordered_map<ep_u64, ep_sp<GL::ProgramInfo>> shaders;
    std::unordered_map<ep_u64, std::unordered_map<std::string, PhiShaderUniform>> shadersDefaultUniforms;

    void loadResources() {
        noteTextures.clear();
        hitEffectTextures.clear();
        storyboardTextures.clear();

        for (const auto type : {
            EnumPhiNoteType::Tap, EnumPhiNoteType::Drag,
            EnumPhiNoteType::Flick, EnumPhiNoteType::Hold
        }) {
            noteTextures[type] = { nullptr, nullptr };
            calcConfig.noteTextureInfos[type] = {};

            for (const auto isSimul : { false, true }) {
                auto loadResult = noteTextureDataLoader(NoteTextureDataLoaderConfig {
                    .type = type,
                    .isSimul = isSimul
                });

                auto decoded = sharedComp.textureDecoder(loadResult.encoded);
                auto tex = glCtx->createTextureFromDecoded(decoded);
                if (!loadResult.cutPaddingIsPixel) loadResult.cutPadding *= decoded.height;
                if (loadResult.ignoreCutPadding) loadResult.cutPadding = Vec2 { (ep_f64)decoded.height, (ep_f64)decoded.height } / 2;

                if (!isSimul) noteTextures[type].first = tex;
                else noteTextures[type].second = tex;

                PhiCalculateFrameConfig::NoteTextureInfo::Item item {
                    .textureSize = Vec2 { (ep_f64)decoded.width, (ep_f64)decoded.height },
                    .cutPadding = loadResult.cutPadding
                };

                if (!isSimul) calcConfig.noteTextureInfos[type].single = item;
                else calcConfig.noteTextureInfos[type].simul = item;
            }

            auto& info = calcConfig.noteTextureInfos[type];
            auto simulScale = (ep_f64)info.simul.textureSize.x / info.single.textureSize.x;
            info.simul.scaling = { simulScale, simulScale };
        }

        auto hitEffectDatas = hitEffectDataLoader();
        for (const auto& data : hitEffectDatas) {
            auto decoded = sharedComp.textureDecoder(data);
            auto tex = glCtx->createTextureFromDecoded(decoded);
            hitEffectTextures.push_back(tex);
        }

        for (const auto type : {
            EnumPhiNoteType::Tap, EnumPhiNoteType::Drag,
            EnumPhiNoteType::Flick, EnumPhiNoteType::Hold
        }) {
            auto data = hitsoundDataLoader(type);
            hitsoundAudios[type] = audioManager.decodeAndCheck(data);
        }
    }
};

enum class EnumMilEventType : ep_u64 {
    PositionX, PositionY,
    Transparency, Size, Rotation,
    FlowSpeed,
    RelativeX, RelativeY,
    LineBodyTransparency, LineHeadTransparency,
    StoryBoardWidth, StoryBoardHeight,
    Speed,
    WholeTransparency,
    StoryBoardLeftBottomX, StoryBoardLeftBottomY,
    StoryBoardRightBottomX, StoryBoardRightBottomY,
    StoryBoardLeftTopX, StoryBoardLeftTopY,
    StoryBoardRightTopX, StoryBoardRightTopY,
    Color,
    VisibleArea,
    MAX = VisibleArea + 1
};

enum class EnumMilObjectType : ep_u64 {
    Line, Note, Storyboard,
    MAX = Storyboard + 1
};

enum class EnumMilNoteType {
    Hit, Drag
};

enum class EnumMilStoryboardType {
    Picture, Text
};

enum class EnumMilStoryboardLayer {
    Background, Normal, Foreground
};

struct MilEventTypeHelper {
    /* !docs
    A helper for converting milthm event type to `@EnumMilEventType`.
    */

    static EnumMilEventType FromInt(ep_u64 type) {
        if (type == 0) return EnumMilEventType::PositionX;
        if (type == 1) return EnumMilEventType::PositionY;
        if (type == 2) return EnumMilEventType::Transparency;
        if (type == 3) return EnumMilEventType::Size;
        if (type == 4) return EnumMilEventType::Rotation;
        if (type == 5) return EnumMilEventType::FlowSpeed;
        if (type == 6) return EnumMilEventType::RelativeX;
        if (type == 7) return EnumMilEventType::RelativeY;
        if (type == 8) return EnumMilEventType::LineBodyTransparency;
        if (type == 9) return EnumMilEventType::LineHeadTransparency;
        if (type == 10) return EnumMilEventType::StoryBoardWidth;
        if (type == 11) return EnumMilEventType::StoryBoardHeight;
        if (type == 12) return EnumMilEventType::Speed;
        if (type == 13) return EnumMilEventType::WholeTransparency;
        if (type == 14) return EnumMilEventType::StoryBoardLeftBottomX;
        if (type == 15) return EnumMilEventType::StoryBoardLeftBottomY;
        if (type == 16) return EnumMilEventType::StoryBoardRightBottomX;
        if (type == 17) return EnumMilEventType::StoryBoardRightBottomY;
        if (type == 18) return EnumMilEventType::StoryBoardLeftTopX;
        if (type == 19) return EnumMilEventType::StoryBoardLeftTopY;
        if (type == 20) return EnumMilEventType::StoryBoardRightTopX;
        if (type == 21) return EnumMilEventType::StoryBoardRightTopY;
        if (type == 22) return EnumMilEventType::Color;
        if (type == 23) return EnumMilEventType::VisibleArea;
        return EnumMilEventType::PositionX;
    }
};

struct MilObjectTypeHelper {
    /* !docs
    A helper for converting milthm object type to `@EnumMilObjectType`.
    */

    static EnumMilObjectType FromInt(ep_u64 type) {
        if (type == 0) return EnumMilObjectType::Line;
        if (type == 1) return EnumMilObjectType::Note;
        if (type == 2) return EnumMilObjectType::Storyboard;
        return EnumMilObjectType::Line;
    }
};

struct MilNoteTypeHelper {
    /* !docs
    A helper for converting milthm note type to `@EnumMilNoteType`.
    */

    static EnumMilNoteType FromInt(ep_u64 type) {
        if (type == 0) return EnumMilNoteType::Hit;
        if (type == 1) return EnumMilNoteType::Drag;
        return EnumMilNoteType::Hit;
    }
};

struct MilStoryboardTypeHelper {
    /* !docs
    A helper for converting milthm storyboard type to `@EnumMilStoryboardType`.
    */

    static EnumMilStoryboardType FromInt(ep_u64 type) {
        if (type == 0) return EnumMilStoryboardType::Picture;
        if (type == 1) return EnumMilStoryboardType::Text;
        return EnumMilStoryboardType::Picture;
    }
};

struct MilStoryboardLayerHelper {
    /* !docs
    A helper for converting milthm storyboard layer to `@EnumMilStoryboardLayer`.
    */

    static EnumMilStoryboardLayer FromInt(ep_u64 type) {
        if (type == 0) return EnumMilStoryboardLayer::Background;
        if (type == 1) return EnumMilStoryboardLayer::Normal;
        if (type == 2) return EnumMilStoryboardLayer::Foreground;
        return EnumMilStoryboardLayer::Background;
    }
};

struct MilMeta {
    /* !docs
    The meta information of a milthm chart.
    */

    std::string title;
    std::string composer;
    std::string artist;
    std::string charter;
    std::string difficultyName;
    ep_f64 difficultyValue;

    enum class NoteFlowSpeedBehavior {
        Override, Multiply, Add
    };

    NoteFlowSpeedBehavior noteFlowSpeedBehavior;

    Vec2 worldOrigin, worldViewport; /* !inline-docs|
    The world origin and viewport of the chart, used to normalize the positions.
    */
};

struct MilBPMEvent {
    /* !docs
    A bpm event item for the milthm chart.
    */

    ep_f64 time; // !inline-docs| in seconds.
    ep_f64 bpm;
};

struct MilEventLayerIndexs {
    /* !docs
    The layer indexs preset of a milthm chart.
    */

    static constexpr ep_u64 UNIT = 1000000;

    static constexpr ep_u64 DEFAULT = UNIT * 1;
};

struct MilEvent {
    Vec2 timeZone; // !inline-docs| in seconds.
    Vec2 valueZone;
    EnumMilEventType type;

    ep_f64 (* easingFunc)(void*, ep_f64);
    ep_f64 (* easingIntFunc)(void*, ep_f64);
    void* easingFuncContext;
    ep_u64 index;

    ep_f64 cumulativeValueAtStart;

    static ep_f64 getDefaultValue(EnumMilObjectType objType, EnumMilEventType eventType) {
        static std::unordered_map<EnumMilObjectType, std::unordered_map<EnumMilEventType, ep_f64>> defaultValues = {
            { EnumMilObjectType::Line, {
                { EnumMilEventType::PositionY, -350 },
                { EnumMilEventType::Transparency, 1 },
                { EnumMilEventType::Size, 1 },
                { EnumMilEventType::Rotation, 90 },
                { EnumMilEventType::FlowSpeed, 1 },
                { EnumMilEventType::LineBodyTransparency, 1 },
                { EnumMilEventType::LineHeadTransparency, 1 },
                { EnumMilEventType::Speed, 1 },
                { EnumMilEventType::WholeTransparency, 1 },
                { EnumMilEventType::Color, 0xffffffff },
                { EnumMilEventType::VisibleArea, (ep_f64)2500 / 1080 }
            } },
            { EnumMilObjectType::Note, {
                { EnumMilEventType::Transparency, 1 },
                { EnumMilEventType::Size, 1 },
                { EnumMilEventType::FlowSpeed, 1 },
                { EnumMilEventType::Color, 0xffffffff }
            } },
            { EnumMilObjectType::Storyboard, {
                { EnumMilEventType::Size, 1 },
                { EnumMilEventType::StoryBoardWidth, 1 },
                { EnumMilEventType::StoryBoardHeight, 1 },
                { EnumMilEventType::StoryBoardLeftBottomX, -0.5 },
                { EnumMilEventType::StoryBoardLeftBottomY, -0.5 },
                { EnumMilEventType::StoryBoardRightBottomX, 0.5 },
                { EnumMilEventType::StoryBoardRightBottomY, -0.5 },
                { EnumMilEventType::StoryBoardLeftTopX, -0.5 },
                { EnumMilEventType::StoryBoardLeftTopY, 0.5 },
                { EnumMilEventType::StoryBoardRightTopX, 0.5 },
                { EnumMilEventType::StoryBoardRightTopY, 0.5 },
                { EnumMilEventType::Color, 0xffffffff }
            } }
        };

        return defaultValues[objType][eventType];
    }

    ep_f64 getProgressAtTime(ep_f64 t) noexcept {
        if (timeZone.isZeroZone()) return 1.0;
        return std::clamp((t - timeZone.x) / (timeZone.y - timeZone.x), 0.0, 1.0);
    }

    ep_f64 valueAtTime(ep_f64 t) noexcept {
        auto p = getProgressAtTime(t);

        if (hasValueEasing()) {
            p = easingFunc(easingFuncContext, p);
        }

        return valueZone.x + p * (valueZone.y - valueZone.x);
    }

    private:
    bool hasValueEasing() const noexcept { return easingFunc != nullptr; }
    bool hasAllEasing() const noexcept { return easingFunc != nullptr && easingIntFunc != nullptr; }
};

struct MilAnimGroup {
    /* !docs
    A animation group for the milthm chart.
    */

    std::vector<MilEvent> events[(ep_u64)EnumMilEventType::MAX];
    EnumMilObjectType objType;

    void addEvent(const MilEvent& e) { events[(ep_u64)e.type].push_back(e); }
    std::vector<MilEvent>& getEvents(EnumMilEventType type) { return events[(ep_u64)type]; }

    void init() {
        for (ep_u64 i = 0; i < (ep_u64)EnumMilEventType::MAX; i++) {
            auto& typedEvents = events[i];

            std::sort(typedEvents.begin(), typedEvents.end(), [](const auto& a, const auto& b) {
                if (a.timeZone.x != b.timeZone.x) return a.timeZone.x < b.timeZone.x;
                if (a.timeZone.y != b.timeZone.y) return a.timeZone.y < b.timeZone.y;
                return a.index < b.index;
            });

            if (typedEvents.empty()) {
                currentValues[i] = MilEvent::getDefaultValue(objType, (EnumMilEventType)i);
            }
        }
    }

    void updateType(ep_u64 type, ep_f64 t) {
        auto& typedEvents = events[type];
        if (typedEvents.empty()) return;

        if (lastUpdatedTimes[type] == t) return;
        if (lastUpdatedTimes[type] > t) currentIndexs[type] = 0;
        
        while (
            currentIndexs[type] < typedEvents.size() - 1
            && typedEvents[currentIndexs[type]].timeZone.y <= t
            && typedEvents[currentIndexs[type] + 1].timeZone.x <= t
        ) currentIndexs[type]++;

        auto& e = typedEvents[currentIndexs[type]];

        if (type == (ep_u64)EnumMilEventType::Speed) {
            // TODO: implement it
        } else {
            currentValues[type] = e.valueAtTime(t);
        }
        
        lastUpdatedTimes[type] = t;
    }

    ep_f64 get(EnumMilEventType type) {
        return currentValues[(ep_u64)type];
    }

    std::optional<ep_f64> getAlwaysValue(EnumMilEventType type) noexcept {
        auto& typedEvents = getEvents(type);
        if (typedEvents.empty()) return MilEvent::getDefaultValue(objType, type);

        if (type == EnumMilEventType::Speed) {
            if (typedEvents.size() == 1 && typedEvents[0].valueZone.isZeroZone()) {
                return typedEvents[0].valueZone.x;
            }

            return std::nullopt;
        }

        ep_f64 fixedValue = typedEvents[0].valueZone.x;
        for (auto& e : typedEvents) {
            if (e.timeZone.isZeroZone()) {
                if (e.valueZone.y != fixedValue) {
                    return std::nullopt;
                }

                continue;
            }

            if (!e.valueZone.isZeroZone() || fixedValue != e.valueZone.x) {
                return std::nullopt;
            }
        }

        return fixedValue;
    }

    private:
    ep_f64 lastUpdatedTimes[(ep_u64)EnumMilEventType::MAX];
    ep_u64 currentIndexs[(ep_u64)EnumMilEventType::MAX];
    ep_f64 currentValues[(ep_u64)EnumMilEventType::MAX];
};

struct MilAnimator {
    /* !docs
    The animator of a milthm chart.
    Like `@PhiAnimator`.
    */

    using ObjDesc = std::pair<EnumMilObjectType, ep_u64>;

    ObjectIndexGenerator<ObjDesc> indexGen;
    std::unordered_map<ep_u64, MilAnimGroup> groups;

    MilAnimGroup& requestGroup(const ObjDesc& obj) {
        auto& ret = groups.try_emplace(indexGen.get(obj), MilAnimGroup { }).first->second;
        ret.objType = obj.first;
        return ret;
    }

    void addEvent(const ObjDesc& obj, const MilEvent& e) {
        requestGroup(obj).addEvent(e);
    }

    void init() {
        for (auto& [_, group] : groups) {
            group.init();
        }
    }

    template <typename T>
    ep_f64 get(T& obj, ep_f64 t, EnumMilEventType type) {
        auto group_it = groups.find(obj.indexer.get());
        if (group_it == groups.end()) return MilEvent::getDefaultValue(T::ObjType, type);

        auto& group = group_it->second;
        group.updateType((ep_u64)type, t);
        return group.get(type);
    }

    template <typename T>
    std::optional<ep_f64> getNoteAnimHash(T& obj) {
        HashBucket hash;

        auto group_it = groups.find(obj.indexer.get());

        for (const auto type : {
            EnumMilEventType::PositionX,
            EnumMilEventType::PositionY,
            EnumMilEventType::Size,
            EnumMilEventType::Rotation,
            EnumMilEventType::FlowSpeed,
            EnumMilEventType::RelativeX,
            EnumMilEventType::RelativeY,
            EnumMilEventType::Speed
        }) {
            if (group_it == groups.end()) {
                hash.submitNumber(MilEvent::getDefaultValue(T::ObjType, type));
            } else {
                auto v = group_it->second.getAlwaysValue(type);
                if (!v.has_value()) return std::nullopt;
                hash.submitNumber(v.value());
            }
        }

        return hash.getHash();
    }
};

struct MilNote {
    /* !docs
    A note of the milthm chart.
    */

    ObjectIndexer indexer;
    static constexpr auto ObjType = EnumMilObjectType::Note;

    struct State {
        ep_f64 lastUpdateTime;
        bool playedHitsound;

        void timeUpdated(ep_f64 t) noexcept {
            if (lastUpdateTime > t) {
                playedHitsound = false;
            }

            lastUpdateTime = t;
        }

        bool onPlayHitsound() noexcept {
            if (!playedHitsound) {
                playedHitsound = true;
                return true;
            }

            return false;
        }
    };

    EnumMilNoteType type;
    Vec2 timeZone;
    bool isFake, isAlwaysPerfect;

    Vec2 floorPosition;
    bool isSimul;

    State state;

    bool isHold() noexcept {
        return !timeZone.isZeroZone() && type == EnumMilNoteType::Hit;
    }
};

struct MilNoteGroup {
    /* !docs
    Like `@PhiNoteGroup`.
    */

    struct State {
        ep_f64 lastUpdateTime;
        ep_u64 firstNoteIndex;

        void timeUpdated(ep_f64 t) noexcept {
            if (lastUpdateTime > t) {
                firstNoteIndex = 0;
            }

            lastUpdateTime = t;
        }

        void passedNoteIndex(ep_u64 index) noexcept {
            if (firstNoteIndex == index) {
                firstNoteIndex++;
            }
        }
    };

    std::vector<ep_u64> indexs;
    bool breakable = true;

    State state;
};

struct MilLine {
    /* !docs
    A line(track?) of the milthm chart.
    */

    ObjectIndexer indexer;
    static constexpr auto ObjType = EnumMilObjectType::Line;

    std::vector<MilNote> notes;

    std::vector<MilNoteGroup> noteGroups;
};

struct MilStoryboardObject {
    /* !docs
    A storyboard object of the milthm chart.
    */

    ObjectIndexer indexer;
    static constexpr auto ObjType = EnumMilObjectType::Storyboard;

    EnumMilStoryboardType type;
    std::string data;
    EnumMilStoryboardLayer layer;
};

struct MilStoryboardAssets {
    /* !docs
    The assets of the storyboard of a milthm chart.
    */
};

struct MilHitEffectItem {
    /* !docs
    A hit effect item for the milthm chart.
    */

    struct Particle {
        ep_f64 rotate;
        ep_f64 initialSpeed;
        ep_f64 initialSize;
        Vec2 scale;
        ep_f64 gravCoeff; // !inline-docs| gravity coefficient.
    };

    ep_f64 time;
    ep_u64 lineIndex, noteIndex;
    std::vector<Particle> particles;
};

struct MilChart {
    /* !docs
    The milthm chart.
    */

    struct State {
        ep_f64 lastUpdateTime;
        ep_u64 firstHitEffectIndex;

        void timeUpdated(ep_f64 t) noexcept {
            if (lastUpdateTime > t) {
                firstHitEffectIndex = 0;
            }

            lastUpdateTime = t;
        }

        void passedHitEffectIndex(ep_u64 index) noexcept {
            if (firstHitEffectIndex == index) {
                firstHitEffectIndex++;
            }
        }
    };

    struct UserOptions {
        ep_f64 noteScaling = 1.0;

        ep_f64 backgroundDim = 0.8;
    };

    MilMeta meta;
    std::vector<MilLine> lines;
    std::vector<MilStoryboardObject> storyboardObjects;
    MilAnimator animator;
    MilStoryboardAssets storyboardAssets;
    
    std::vector<MilHitEffectItem> hitEffects;
    std::vector<ep_f64> comboTimes;
    ep_u64 rawHash;

    UserOptions options;

    State state;

    void init() {
        animator.init();
    }

    Vec2 getLinePosition(ep_f64 t, MilLine& line, const Vec2& screenSize) {
        Vec2 pos = {
            animator.get(line, t, EnumMilEventType::PositionX) + animator.get(line, t, EnumMilEventType::RelativeX),
            animator.get(line, t, EnumMilEventType::PositionY) + animator.get(line, t, EnumMilEventType::RelativeY)
        };

        return (pos - meta.worldOrigin) / meta.worldViewport * screenSize;
    }
};

struct MilChartLoadResult {
    bool success;
    std::vector<std::string> errors;
    MilChart chart;
};

#define CHART_LOAD_FAILED(prefix, err) \
    { \
        return MilChartLoadResult { \
            .success = false, \
            .errors = { std::string(prefix) + ": " + (err) } \
        }; \
    }

MilChartLoadResult loadMilChartFromDevJson(const Data& data) {
    JsonNode jsonRoot;
    auto [jsonParseSuccess, err] = JsonNode::Parse(&jsonRoot, data);
    if (!jsonParseSuccess) CHART_LOAD_FAILED("dev", std::string("failed to parse json: ") + err);

    MilChart chart {};
    chart.meta.noteFlowSpeedBehavior = MilMeta::NoteFlowSpeedBehavior::Override;
    chart.meta.worldOrigin = { (ep_f64)-1920 / 2, (ep_f64)1080 / 2 };
    chart.meta.worldViewport = { 1920, -1080 };

    if (!jsonRoot.isObject()) CHART_LOAD_FAILED("dev", "root is not an object");

    if (!jsonRoot.hasKey("meta")) CHART_LOAD_FAILED("dev", "missing meta field");
    if (!jsonRoot["meta"].isObject()) CHART_LOAD_FAILED("dev", "meta is not an object");

    auto& metaNode = jsonRoot["meta"];

    if (!metaNode.hasKey("Title")) CHART_LOAD_FAILED("dev", "missing Title field");
    if (!metaNode["Title"].isString()) CHART_LOAD_FAILED("dev", "Title is not a string");
    chart.meta.title = metaNode["Title"].getString();

    if (!metaNode.hasKey("Composer")) CHART_LOAD_FAILED("dev", "missing Composer field");
    if (!metaNode["Composer"].isString()) CHART_LOAD_FAILED("dev", "Composer is not a string");
    chart.meta.composer = metaNode["Composer"].getString();

    if (!metaNode.hasKey("Illustrator")) CHART_LOAD_FAILED("dev", "missing Illustrator field");
    if (!metaNode["Illustrator"].isString()) CHART_LOAD_FAILED("dev", "Illustrator is not a string");
    chart.meta.artist = metaNode["Illustrator"].getString();

    if (!metaNode.hasKey("Beatmapper")) CHART_LOAD_FAILED("dev", "missing Beatmapper field");
    if (!metaNode["Beatmapper"].isString()) CHART_LOAD_FAILED("dev", "Beatmapper is not a string");
    chart.meta.charter = metaNode["Beatmapper"].getString();

    if (!metaNode.hasKey("Difficulty")) CHART_LOAD_FAILED("dev", "missing Difficulty field");
    if (!metaNode["Difficulty"].isString()) CHART_LOAD_FAILED("dev", "Difficulty is not a string");
    chart.meta.difficultyName = metaNode["Difficulty"].getString();

    if (!metaNode.hasKey("DifficultyValue")) CHART_LOAD_FAILED("dev", "missing DifficultyValue field");
    if (!metaNode["DifficultyValue"].isNumber()) CHART_LOAD_FAILED("dev", "DifficultyValue is not a number");
    chart.meta.difficultyValue = metaNode["DifficultyValue"].getNumber();

    std::vector<MilBPMEvent> bpms;

    if (!jsonRoot.hasKey("bpms")) CHART_LOAD_FAILED("dev", "missing bpms field");
    if (!jsonRoot["bpms"].isArray()) CHART_LOAD_FAILED("dev", "bpms is not an array");

    for (auto& bpmNode : jsonRoot["bpms"].getArray()) {
        if (!bpmNode.isObject()) CHART_LOAD_FAILED("dev", "bpm is not an object");

        if (!bpmNode.hasKey("start")) CHART_LOAD_FAILED("dev", "missing start field");
        if (!bpmNode["start"].isNumber()) CHART_LOAD_FAILED("dev", "start is not a number");
        ep_f64 start = bpmNode["start"].getNumber();

        if (!bpmNode.hasKey("bpm")) CHART_LOAD_FAILED("dev", "missing bpm field");
        if (!bpmNode["bpm"].isNumber()) CHART_LOAD_FAILED("dev", "bpm is not a number");
        ep_f64 bpm = bpmNode["bpm"].getNumber();

        bpms.push_back({ .time = start, .bpm = bpm });
    }

    auto cvtTime = [&](JsonNode& node, const std::string& key, ep_u64 bpm, ep_f64* dst) {
        if (!node.hasKey(key)) return false;
        auto& timeNode = node[key];

        if (timeNode.isNumber()) {
            *dst = timeNode.getNumber();
            return true;
        }

        if (timeNode.isArray()) {
            auto& timeArray = timeNode.getArray();
            if (timeArray.size() != 3 && timeArray.size() != 4) return false;

            if (!timeArray[0].isNumber()) return false;
            if (!timeArray[1].isNumber()) return false;
            if (!timeArray[2].isNumber()) return false;
            if (timeArray.size() == 4 && !timeArray[3].isNumber()) return false;

            ep_f64 beatTime = timeArray[0].getNumber() + timeArray[1].getNumber() / timeArray[2].getNumber();
            auto& bpmEvent = bpms[timeArray.size() == 3 ? bpm : (ep_u64)timeArray[3].getNumber()];

            *dst = bpmEvent.time + beatTime * (60.0 / bpmEvent.bpm);
            return true;
        }

        return false;
    };

    if (!jsonRoot.hasKey("lines")) CHART_LOAD_FAILED("dev", "missing lines field");
    if (!jsonRoot["lines"].isArray()) CHART_LOAD_FAILED("dev", "lines is not an array");

    ep_u64 lineIndex = 0;
    for (auto& lineNode : jsonRoot["lines"].getArray()) {
        if (!lineNode.isObject()) CHART_LOAD_FAILED("dev", "line is not an object");

        auto& line = chart.lines.emplace_back();
        line.indexer.set(chart.animator.indexGen.get({ EnumMilObjectType::Line, lineIndex++ }));

        if (!lineNode.hasKey("notes")) CHART_LOAD_FAILED("dev", "missing notes field");
        if (!lineNode["notes"].isArray()) CHART_LOAD_FAILED("dev", "notes is not an array");

        for (auto& noteNode : lineNode["notes"].getArray()) {
            if (!noteNode.isObject()) CHART_LOAD_FAILED("dev", "note is not an object");

            auto& note = line.notes.emplace_back();

            if (!noteNode.hasKey("bpm")) CHART_LOAD_FAILED("dev", "missing bpm field");
            if (!noteNode["bpm"].isNumber()) CHART_LOAD_FAILED("dev", "bpm is not a number");
            ep_u64 bpm = noteNode["bpm"].getNumber();

            if (!cvtTime(noteNode, "startTime", bpm, &note.timeZone.x)) CHART_LOAD_FAILED("dev", "invalid startTime");
            if (!cvtTime(noteNode, "endTime", bpm, &note.timeZone.y)) CHART_LOAD_FAILED("dev", "invalid endTime");

            if (!noteNode.hasKey("type")) CHART_LOAD_FAILED("dev", "missing type field");
            if (!noteNode["type"].isNumber()) CHART_LOAD_FAILED("dev", "type is not a number");
            note.type = MilNoteTypeHelper::FromInt(noteNode["type"].getNumber());

            if (!noteNode.hasKey("isFake")) CHART_LOAD_FAILED("dev", "missing isFake field");
            if (!noteNode["isFake"].isBool()) CHART_LOAD_FAILED("dev", "isFake is not a bool");
            note.isFake = noteNode["isFake"].getBool();

            if (!noteNode.hasKey("isAlwaysPerfect")) CHART_LOAD_FAILED("dev", "missing isAlwaysPerfect field");
            if (!noteNode["isAlwaysPerfect"].isBool()) CHART_LOAD_FAILED("dev", "isAlwaysPerfect is not a bool");
            note.isAlwaysPerfect = noteNode["isAlwaysPerfect"].getBool();

            if (!noteNode.hasKey("index")) CHART_LOAD_FAILED("dev", "missing index field");
            if (!noteNode["index"].isNumber()) CHART_LOAD_FAILED("dev", "index is not a number");
            ep_u64 noteIndex = noteNode["index"].getNumber();

            note.indexer.set(chart.animator.indexGen.get({ EnumMilObjectType::Note, noteIndex }));
        }
    }

    if (!jsonRoot.hasKey("storyboardObjects")) CHART_LOAD_FAILED("dev", "missing storyboardObjects field");
    if (!jsonRoot["storyboardObjects"].isArray()) CHART_LOAD_FAILED("dev", "storyboardObjects is not an array");

    ep_u64 sbIndex = 0;
    for (auto& sbNode : jsonRoot["storyboardObjects"].getArray()) {
        if (!sbNode.isObject()) CHART_LOAD_FAILED("dev", "storyboardObject is not an object");

        auto& sb = chart.storyboardObjects.emplace_back();
        sb.indexer.set(chart.animator.indexGen.get({ EnumMilObjectType::Storyboard, sbIndex++ }));

        if (!sbNode.hasKey("type")) CHART_LOAD_FAILED("dev", "missing type field");
        if (!sbNode["type"].isNumber()) CHART_LOAD_FAILED("dev", "type is not a number");
        sb.type = MilStoryboardTypeHelper::FromInt(sbNode["type"].getNumber());

        if (!sbNode.hasKey("data")) CHART_LOAD_FAILED("dev", "missing data field");
        if (!sbNode["data"].isString()) CHART_LOAD_FAILED("dev", "data is not an object");
        sb.data = sbNode["data"].getString();

        if (!sbNode.hasKey("layer")) CHART_LOAD_FAILED("dev", "missing layer field");
        if (!sbNode["layer"].isNumber()) CHART_LOAD_FAILED("dev", "layer is not a number");
        sb.layer = MilStoryboardLayerHelper::FromInt(sbNode["layer"].getNumber());
    }

    auto cvtAnimVal = [](JsonNode& node, const std::string& key, ep_f64* dst) {
        if (!node.hasKey(key)) return false;

        auto& valNode = node[key];

        if (valNode.isNumber()) {
            *dst = valNode.getNumber();
            return true;
        } else if (valNode.isString()) {
            auto& str = valNode.getString();

            if (str.empty()) {
                *dst = 0.0;
                return true;
            }

            try { *dst = std::stod(str); }
            catch (...) { return false; }
            return true;
        }

        return false;
    };

    if (!jsonRoot.hasKey("animations")) CHART_LOAD_FAILED("dev", "missing animations field");
    if (!jsonRoot["animations"].isArray()) CHART_LOAD_FAILED("dev", "animations is not an array");

    ep_u64 eventIndex = 0;
    for (auto& animNode : jsonRoot["animations"].getArray()) {
        if (!animNode.isObject()) CHART_LOAD_FAILED("dev", "animation is not an object");

        MilEvent e {};
        e.index = eventIndex++;

        if (!animNode.hasKey("bpmId")) CHART_LOAD_FAILED("dev", "missing bpmId field");
        if (!animNode["bpmId"].isNumber()) CHART_LOAD_FAILED("dev", "bpmId is not a number");
        ep_u64 bpm = animNode["bpmId"].getNumber();

        if (!cvtTime(animNode, "fromBeat", bpm, &e.timeZone.x)) CHART_LOAD_FAILED("dev", "invalid fromBeat");
        if (!cvtTime(animNode, "toBeat", bpm, &e.timeZone.y)) CHART_LOAD_FAILED("dev", "invalid toBeat");

        if (!animNode.hasKey("key")) CHART_LOAD_FAILED("dev", "missing key field");
        if (!animNode["key"].isNumber()) CHART_LOAD_FAILED("dev", "key is not a string");
        e.type = MilEventTypeHelper::FromInt(animNode["key"].getNumber());

        if (e.type == EnumMilEventType::Color) {
            continue; // TODO: implement it
        } else {
            if (!cvtAnimVal(animNode, "fv", &e.valueZone.x)) CHART_LOAD_FAILED("dev", "invalid fv");
            if (!cvtAnimVal(animNode, "tv", &e.valueZone.y)) CHART_LOAD_FAILED("dev", "invalid tv");
        }

        if (!animNode.hasKey("data")) CHART_LOAD_FAILED("dev", "missing data field");
        if (!animNode["data"].isNumber()) CHART_LOAD_FAILED("dev", "data is not a number");
        auto objType = MilObjectTypeHelper::FromInt(animNode["data"].getNumber());

        if (!animNode.hasKey("i1")) CHART_LOAD_FAILED("dev", "missing i1 field");
        if (!animNode["i1"].isNumber()) CHART_LOAD_FAILED("dev", "i1 is not a number");
        ep_u64 objIndex = animNode["i1"].getNumber();

        if (!animNode.hasKey("ease")) CHART_LOAD_FAILED("dev", "missing ease field");
        if (!animNode["ease"].isNumber()) CHART_LOAD_FAILED("dev", "ease is not a number");
        ep_u64 ease = std::clamp<ep_f64>(animNode["ease"].getNumber(), 0, 2);

        if (!animNode.hasKey("press")) CHART_LOAD_FAILED("dev", "missing press field");
        if (!animNode["press"].isNumber()) CHART_LOAD_FAILED("dev", "press is not a number");
        ep_u64 press = std::clamp<ep_f64>(animNode["press"].getNumber(), 0, 10);

        if (press != 0) {
            e.easingFuncContext = (void*)(ease << 4 | press);
            e.easingFunc = [](void* context, ep_f64 p) {
                ep_u64 ctx = (ep_u64)context;
                ep_u64 ease = ctx >> 4;
                ep_u64 press = ctx & 0b1111;

                return EaseSet::Milthm::easing(ease, press, p);
            };
        }

        chart.animator.addEvent({ objType, objIndex }, e);
    }

    chart.rawHash = data.getHash();

    return MilChartLoadResult {
        .success = true,
        .chart = chart
    };
}

MilChartLoadResult loadMilChartFromData(const Data& data) {
    MilChartLoadResult result {};
    result.success = false;

    #define TRY_LOAD_FUNC(func) \
        { \
            auto res = func(data); \
            if (res.success) return res; \
            result.errors.insert(result.errors.end(), res.errors.begin(), res.errors.end()); \
        }
    
    TRY_LOAD_FUNC(loadMilChartFromDevJson);

    return result;
    
    #undef TRY_LOAD_FUNC
}

struct MilCalculateFrameConfig {
    Vec2 screenSize;
    Vec2 backgroundTextureSize;
    ep_f64 songLength;
    ep_f64 lineHeadScale = 1.0;
    ep_f64 lineHeadConnectPoint;
};

struct MilCalculatedFrame {
    using CalculatedText = SharedCalculatedObjects::CalculatedText;
    using CalculatedRect = SharedCalculatedObjects::CalculatedRect;
    using CalculatedPoly = SharedCalculatedObjects::CalculatedPoly;

    struct CalculatedLineHead {
        Vec2 position, scale;
        ep_f64 size;
        ep_f64 rotation;
        Color color;
    };

    using CalculatedObject = std::variant<
        CalculatedText,
        CalculatedRect,
        CalculatedPoly,
        CalculatedLineHead
    >;

    Rect backgroundRect;
    ep_f64 backgroundDim;
    Rect progressbarRect;
    std::vector<CalculatedObject> objects;

    struct Cache {
        void clear() {

        }
    };

    Cache cache;
    Vec2 frameTimeRange;
};

void calculateMilFrame(
    MilChart& chart, ep_f64 time,
    const MilCalculateFrameConfig& config,
    MilCalculatedFrame& frame
) {
    frame.objects.clear();
    frame.cache.clear();

    frame.frameTimeRange = { frame.frameTimeRange.y, time };

    frame.backgroundRect = getCoveredOrContainRect(
        { 0.0, 0.0, config.screenSize.x, config.screenSize.y },
        config.backgroundTextureSize, true
    );
    frame.backgroundDim = chart.options.backgroundDim;

    frame.progressbarRect = {
        0.0, 0.0,
        time / config.songLength * config.screenSize.x,
        config.screenSize.x * 0.0046875
    };

    ep_f64 lineHeadBase = config.screenSize.sum() * 0.0223;

    for (auto& line : chart.lines) {
        auto linePosition = chart.getLinePosition(time, line, config.screenSize);
        auto lineRotation = chart.animator.get(line, time, EnumMilEventType::Rotation);
        auto lineAlpha = chart.animator.get(line, time, EnumMilEventType::Transparency);
        auto lineHeadAlpha = chart.animator.get(line, time, EnumMilEventType::LineHeadTransparency);
        auto lineBodyAlpha = chart.animator.get(line, time, EnumMilEventType::LineBodyTransparency);
        auto lineSize = chart.animator.get(line, time, EnumMilEventType::Size);

        lineHeadAlpha *= lineAlpha;
        lineBodyAlpha *= lineAlpha;

        if (lineHeadAlpha > 0.0) {
            frame.objects.push_back(MilCalculatedFrame::CalculatedLineHead {
                .position = linePosition,
                .scale = { lineSize, lineSize },
                .size = lineHeadBase * config.lineHeadScale,
                .rotation = lineRotation,
                .color = { 1.0, 1.0, 1.0, lineHeadAlpha }
            });
        }

        if (lineBodyAlpha > 0.0) {
            auto connectRadius = config.lineHeadConnectPoint * lineHeadBase * config.lineHeadScale;
            auto lineWidth = lineHeadBase * 0.096774;
            frame.objects.push_back(PhiCalculatedFrame::CalculatedPoly::Make(
                { connectRadius, -lineWidth / 2 },
                { config.screenSize.y * 2.5, lineWidth },
                { 1.0, 1.0, 1.0, lineBodyAlpha },
                Transform2D()
                    .translate(linePosition)
                    .rotateDegrees(lineRotation + 180)
                    .scale(lineSize)
            ));
        }
    }
}

DecodedRGBATexture spwanMilBackgroundMask() {
    auto tex = DecodedRGBATexture::Make(2, 128);
    const ep_f64 start = 0.2;
    const ep_f64 alphaExp = 0.8;

    for (ep_u64 i = 0; i < tex.width; i++) {
        for (ep_u64 j = 0; j < tex.height; j++) {
            ep_f64 p = (ep_f64)j / (tex.height - 1);
            p = p * (start + 1) - start;
            if (p <= 0.0) continue;
            tex.data[tex.getIndexBase(i, j) + 3] = typed_clamp<ep_u8, ep_f64>(std::pow(p, alphaExp) * 270);
        }
    }

    return tex;
}

DecodedRGBATexture spwanMilProgressbar() {
    auto tex = DecodedRGBATexture::Make(128, 2);
    std::fill(tex.data.begin(), tex.data.end(), 255);

    for (ep_u64 i = 0; i < tex.width; i++) {
        for (ep_u64 j = 0; j < tex.height; j++) {
            ep_f64 p = (ep_f64)i / (tex.width - 1);
            p = 1.0 - std::pow(1.0 - p, 2.2);
            tex.data[tex.getIndexBase(i, j) + 3] = typed_clamp<ep_u8, ep_f64>(p * 255);
        }
    }

    return tex;
}

struct MilTakeOverer {
    MilTakeOverer() = default;
    MilTakeOverer(const MilTakeOverer&) = delete;
    MilTakeOverer& operator=(const MilTakeOverer&) = delete;
    MilTakeOverer(MilTakeOverer&&) = default;
    MilTakeOverer& operator=(MilTakeOverer&&) = default;

    static ep_sp<MilTakeOverer> Make() {
        auto* tor = new MilTakeOverer();
        return ep_sp<MilTakeOverer>(tor);
    }

    ep_sp<GL::GL33Context> glCtx;
    TakeOvererComponents::SharedComp sharedComp;
    GL::TextManager textManager;
    TakeOvererComponents::AudioManager audioManager;
    
    struct LineHeadTextureLoaderResult {
        Data encoded;
        ep_f64 scale = 1.0, connectPoint;
        bool connectPointIsPixel = true;
    };

    using LineHeadTextureLoader = std::function<LineHeadTextureLoaderResult()>;
    LineHeadTextureLoader lineHeadTextureLoader;

    MilCalculateFrameConfig calcConfig;
    MilChart chart;
    MilCalculatedFrame calculatedFrame;

    void init() {
        checkBoolAndThrow(!!glCtx, "glCtx is not set");
        checkBoolAndThrow(!!lineHeadTextureLoader, "lineHeadTextureLoader is not set");

        textManager.glCtx = glCtx;

        sharedComp.check();
        textManager.check();
        audioManager.check();

        loadResources();
    }

    void loadIllustion(const Data& data) {
        auto decoded = sharedComp.textureDecoder(data);
        sharedComp.illustionTexture = glCtx->createTextureFromDecoded(decoded);
    }

    void loadIllustion(const std::string& path) { loadIllustion(Data::MakeFromFile(path)); }

    using ChartIniter = std::function<void(MilChart&)>;

    TakeOvererComponents::LoadChartResultInfo loadChart(
        const Data& data,
        ChartIniter initer = [](MilChart& chart) { chart.init(); }
    ) {
        TakeOvererComponents::LoadChartResultInfo resultInfo {};

        {
            Timer timer;
            auto loadResult = loadMilChartFromData(data);
            resultInfo.createObjectTook = timer.elapsed();

            if (!loadResult.success) {
                resultInfo.success = false;
                resultInfo.errors = std::move(loadResult.errors);
                return resultInfo;
            }

            chart = std::move(loadResult.chart);
        }

        {
            Timer timer;
            initer(chart);
            resultInfo.initTook = timer.elapsed();
        }

        return resultInfo;
    }

    struct RenderConfig {
        TakeOvererComponents::RenderConfigBase base;
    };

    struct RenderResultInfo {
        TakeOvererComponents::RenderResultInfoBase base;
    };

    RenderResultInfo& render(const RenderConfig& renderConfig) {
        calcConfig.songLength = audioManager.getBgmLength();
        calcConfig.backgroundTextureSize = { sharedComp.illustionTexture->width, sharedComp.illustionTexture->height };

        auto t = renderConfig.base.getTime(audioManager);

        {
            Timer timer;
            calculateMilFrame(chart, t, calcConfig, calculatedFrame);
            renderResultInfoCache.base.calculatedTook = timer.elapsed();
        }

        Timer glOpsTimer;

        using namespace GL;

        glCtx->setViewport(calcConfig.screenSize.x, calcConfig.screenSize.y);
        glCtx->setClearColor(0.0, 0.0, 0.0, 0.0);
        glCtx->clear(GL_COLOR_BUFFER_BIT);

        auto cvs = GL33Canvas::Make(glCtx.get());

        cvs.drawRect({
            .position = { calculatedFrame.backgroundRect.x, calculatedFrame.backgroundRect.y },
            .size = { calculatedFrame.backgroundRect.w, calculatedFrame.backgroundRect.h },
            .color = GLvec4::Gray(1.0 - calculatedFrame.backgroundDim),
            .texture = sharedComp.illustionTexture.get()
        });

        cvs.drawRect({
            .position = { calculatedFrame.backgroundRect.x, calculatedFrame.backgroundRect.y },
            .size = { calculatedFrame.backgroundRect.w, calculatedFrame.backgroundRect.h },
            .texture = backgroundMask.get()
        });

        for (auto& obj : calculatedFrame.objects) {
            if (TakeOvererComponents::renderSharedObject(obj, glCtx, cvs, textManager)) {
                continue;
            }

            if (std::holds_alternative<MilCalculatedFrame::CalculatedLineHead>(obj)) {
                auto& lineHead = std::get<MilCalculatedFrame::CalculatedLineHead>(obj);

                cvs.save();
                cvs.translate(lineHead.position);
                cvs.rotateDegrees(lineHead.rotation);
                cvs.scale(lineHead.scale);
                cvs.drawRect({
                    .position = -Vec2 { lineHead.size, lineHead.size } / 2,
                    .size = { lineHead.size, lineHead.size },
                    .color = lineHead.color,
                    .texture = lineHeadTex.get()
                });
                cvs.restore();
            }
        }

        cvs.drawRect({
            .position = { calculatedFrame.progressbarRect.x, calculatedFrame.progressbarRect.y },
            .size = { calculatedFrame.progressbarRect.w, calculatedFrame.progressbarRect.h },
            .texture = progressbarTex.get()
        });

        if (renderConfig.base.flushGl) {
            glCtx->flush();
        }

        renderResultInfoCache.base.glOperationsTook = glOpsTimer.elapsed();

        return renderResultInfoCache;
    }

    private:
    RenderResultInfo renderResultInfoCache;
    ep_sp<GL::TextureInfo> backgroundMask;
    ep_sp<GL::TextureInfo> progressbarTex;
    ep_sp<GL::TextureInfo> lineHeadTex;

    void loadResources() {
        backgroundMask = glCtx->createTextureFromDecoded(spwanMilBackgroundMask());
        progressbarTex = glCtx->createTextureFromDecoded(spwanMilProgressbar());

        {
            auto lineHead = lineHeadTextureLoader();
            auto decoded = sharedComp.textureDecoder(lineHead.encoded);
            lineHeadTex = glCtx->createTextureFromDecoded(decoded);

            calcConfig.lineHeadScale = lineHead.scale;
            if (lineHead.connectPointIsPixel) lineHead.connectPoint /= decoded.height;
            calcConfig.lineHeadConnectPoint = lineHead.connectPoint;
        }
    }
};

#undef CHART_LOAD_FAILED

} // namespace easy_phi

#ifdef EASY_PHI_TEXT_RENDERER
#define STB_TRUETYPE_IMPLEMENTATION
#include "helpers/stb_truetype.h"
namespace easy_phi {
    struct TextRenderer {
        /* !docs
        A text renderer by `stb_truetype`.
        */

        TextRenderer() = default;
        TextRenderer(const TextRenderer&) = delete;
        TextRenderer& operator=(const TextRenderer&) = delete;

        static ep_sp<TextRenderer> Make() {
            auto* tr = new TextRenderer();
            return ep_sp<TextRenderer>(tr);
        }

        void loadFont(const Data& data, ep_u64 index = 0) {
            /* !docs
            Load a font from a data.
            */

            fontData = data;
            if (!stbtt_InitFont(&font, fontData.data.data(), stbtt_GetFontOffsetForIndex(fontData.data.data(), index))) {
                throw std::runtime_error("failed to load font");
            }
        }

        DecodedRGBATexture render(const std::string& text, ep_u64 fontSize) {
            /* !docs
            Render a text to a texture.
            */

            struct DrawedChar {
                DecodedRGBATexture tex;
                ep_i32 xoff, yoff;
                ep_f64 advance_width;
            };

            std::vector<DrawedChar> chars;

            auto scale = stbtt_ScaleForPixelHeight(&font, fontSize);

            for (ep_u64 i = 0; i < text.size(); i++) {
                ep_u64 codepoint = 0;
                ep_u8 bytes = 0;

                auto c = text[i];
                if ((c & 0x80) == 0) {
                    codepoint = c;
                    bytes = 1;
                } else if ((c & 0xE0) == 0xC0) {
                    codepoint = c & 0x1F;
                    bytes = 2;
                } else if ((c & 0xF0) == 0xE0) {
                    codepoint = c & 0x0F;
                    bytes = 3;
                } else if ((c & 0xF8) == 0xF0) {
                    codepoint = c & 0x07;
                    bytes = 4;
                }

                if (i + bytes > text.size()) break;

                for (ep_u8 j = 1; j < bytes; j++) {
                    codepoint = (codepoint << 6) | (text[i + j] & 0x3F);
                }

                i += bytes - 1;

                auto glyph_index = stbtt_FindGlyphIndex(&font, codepoint);
                if (!glyph_index) glyph_index = stbtt_FindGlyphIndex(&font, '?');
                if (!glyph_index) continue;

                auto& dc = chars.emplace_back();

                ep_i32 advance, lsb;
                stbtt_GetGlyphHMetrics(&font, glyph_index, &advance, &lsb);
                dc.advance_width = advance * scale;

                ep_i32 w, h;
                stbtt_GetGlyphBitmapBox(&font, glyph_index, scale, scale, &dc.xoff, &dc.yoff, &w, &h);
                w -= dc.xoff; h -= dc.yoff;

                std::vector<ep_u8> bitmap(w * h);
                stbtt_MakeGlyphBitmap(&font, bitmap.data(), w, h, w, scale, scale, glyph_index);

                dc.tex = DecodedRGBATexture::Make(w, h);
                dc.tex.fillWithGray(bitmap);
            }

            if (chars.empty()) return DecodedRGBATexture::Make(2, 2);

            ep_i32 top = 0, bottom = 0;
            ep_f64 width = 0, real_right = 0;

            for (auto& dc : chars) {
                top = std::min(top, dc.yoff);
                bottom = std::max<ep_i32>(bottom, dc.yoff + dc.tex.height);
                real_right = std::max(real_right, width + dc.xoff + dc.tex.width);
                width += dc.advance_width;
                real_right = std::max(real_right, width);
            }

            ep_i32 padding = 2;
            ep_i32 iwidth = std::ceil(real_right - chars.front().xoff + padding * 2);

            if (top >= bottom || iwidth <= 0) return DecodedRGBATexture::Make(2, 2);

            auto tex = DecodedRGBATexture::Make(iwidth, bottom - top);
            tex.fillRGBWhite();

            ep_f64 x = -chars.front().xoff + padding;

            for (auto& dc : chars) {
                ep_i64 y = dc.yoff - top;
                ep_i64 ix = std::ceil(x + dc.xoff);
                tex.paste(dc.tex, ix, y);
                x += dc.advance_width;
            }

            return tex;
        }

        private:
        Data fontData;
        stbtt_fontinfo font;
    };
}
#endif // EASY_PHI_TEXT_RENDERER

#ifdef EASY_PHI_IMAGE_DECODER
#define STB_IMAGE_IMPLEMENTATION
#include "helpers/stb_image.h"
namespace easy_phi {
    DecodedRGBATexture decodeImage(const Data& data) {
        /* !docs
        Decode an image from a data by `stb_image`.
        */

        int width, height, channels;
        ep_u8* pixels = stbi_load_from_memory(
            data.data.data(),
            data.data.size(),
            &width, &height, &channels,
            4
        );

        if (!pixels) {
            throw std::runtime_error("failed to decode image");
        }

        DecodedRGBATexture tex {
            .data = std::vector<ep_u8>(pixels, pixels + width * height * 4),
            .width = (ep_u64)width,
            .height = (ep_u64)height
        };

        stbi_image_free(pixels);
        return tex;
    }
}
#endif // EASY_PHI_IMAGE_DECODER

#ifdef EASY_PHI_MINIAUDIO_AUDIO_ENGINE
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#define MINIAUDIO_IMPLEMENTATION
#include "helpers/miniaudio.h"
#include "helpers/stb_vorbis.c"
namespace easy_phi {
    ep_sp<DecodedAudio> decodeAudioMiniaudio(const Data& data) {
        /* !docs
        Decode an audio from a data by `miniaudio` and `stb_vorbis`.
        */

        if (data.isStartsWith("OggS")) {
            int channels, sampleRate;
            ep_i16* pcm;

            ep_u64 frames = stb_vorbis_decode_memory(
                data.data.data(),
                data.data.size(),
                &channels,
                &sampleRate,
                &pcm
            );

            if (frames <= 0) {
                throw std::runtime_error("failed to decode audio");
            }

            auto decoded = DecodedAudio::Make();
            decoded->channels = channels;
            decoded->sampleRate = sampleRate;
            decoded->data.insert(decoded->data.end(), pcm, pcm + frames * channels);
            free(pcm);
            return decoded;
        }

        ma_decoder_config config = ma_decoder_config_init(ma_format_s16, 0, 0);

        ma_decoder decoder;
        ma_result result = ma_decoder_init_memory(data.data.data(), data.data.size(), &config, &decoder);
        if (result != MA_SUCCESS) {
            throw std::runtime_error("failed to decode audio");
        }

        auto decoded = DecodedAudio::Make();
        decoded->channels = decoder.outputChannels;
        decoded->sampleRate = decoder.outputSampleRate;

        const ep_u64 chunk_size = 4096;
        std::vector<ep_i16> buffer(chunk_size * decoder.outputChannels);

        while (true) {
            ma_uint64 framesRead = 0;

            ma_result result = ma_decoder_read_pcm_frames(
                &decoder, buffer.data(),
                chunk_size, &framesRead
            );

            decoded->data.insert(decoded->data.end(), buffer.begin(), buffer.begin() + framesRead * decoder.outputChannels);
            if (framesRead < chunk_size) break;
        }

        ma_decoder_uninit(&decoder);
        return decoded;
    }

    ep_sp<AudioEngine> makeAudioEngineMiniaudio() {
        /* !docs
        Create an audio engine which is based on `miniaudio`.
        */
        
        auto engine = AudioEngine::Make();

        struct AudioContext {
            ma_device device;
            AudioEngine* engine;

            static ep_sp<AudioContext> Make() {
                auto* ctx = new AudioContext();
                return ep_sp<AudioContext>(ctx);
            }
        };

        auto ctx = AudioContext::Make();

        ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
        deviceConfig.playback.format = ma_format_s16;
        deviceConfig.playback.channels = deviceConfig.sampleRate = 0;
        deviceConfig.dataCallback = [](ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
            auto* ctx = (AudioContext*)pDevice->pUserData;
            ctx->engine->callback((ep_i16*)pOutput, frameCount);
        };
        deviceConfig.pUserData = ctx.get();

        if (ma_device_init(NULL, &deviceConfig, &ctx->device) != MA_SUCCESS) {
            throw std::runtime_error("failed to initialize audio device");
        }
        engine->channels = ctx->device.playback.channels;
        engine->sampleRate = ctx->device.sampleRate;

        ctx->engine = engine.get();
        ma_device_start(&ctx->device);

        engine->audioContext = ctx.release();
        engine->audioContextDestructor = [](void* userdata) {
            auto* ctx = (AudioContext*)userdata;
            ma_device_uninit(&ctx->device);
            delete ctx;
        };

        return engine;
    }
}
#endif // EASY_PHI_MINIAUDIO_AUDIO_ENGINE

#ifdef EASY_PHI_PHI_RESOURCE
namespace easy_phi {
    #include "helpers/resources/phigros.cpp"

    struct PhiStaticResourceHelpers {
        static PhiTakeOverer::NoteTextureDataLoaderResult noteTextureDataLoader(const PhiTakeOverer::NoteTextureDataLoaderConfig& config) {
            std::unordered_map<EnumPhiNoteType, std::string> nameMap = {
                { EnumPhiNoteType::Tap, "click" },
                { EnumPhiNoteType::Drag, "drag" },
                { EnumPhiNoteType::Flick, "flick" },
                { EnumPhiNoteType::Hold, "hold" }
            };

            auto name = nameMap.at(config.type);
            auto key = std::string("/notes/") + name + (config.isSimul ? "_mh.png" : ".png");
            auto data = PhiStaticResource::get(key);

            double cutPadding = config.isSimul ? 100.0 : 50.0;

            return {
                .encoded = std::move(data),
                .cutPadding = Vec2 { cutPadding, cutPadding },
                .cutPaddingIsPixel = true,
                .ignoreCutPadding = config.type != EnumPhiNoteType::Hold
            };
        }

        static std::vector<Data> hitEffectDataLoader() {
            std::vector<Data> result;

            for (ep_u64 i = 0; i < 60; i++) {
                auto key = std::string("/hittexs/") + std::to_string(i + 1) + ".png";
                result.push_back(PhiStaticResource::get(key));
            }

            return result;
        }

        static Data hitsoundDataLoader(EnumPhiNoteType type) {
            std::unordered_map<EnumPhiNoteType, std::string> nameMap = {
                { EnumPhiNoteType::Tap, "click" },
                { EnumPhiNoteType::Drag, "drag" },
                { EnumPhiNoteType::Flick, "flick" },
                { EnumPhiNoteType::Hold, "click" }
            };

            auto name = nameMap.at(type);
            auto key = std::string("/hitsounds/") + name + ".wav";
            return PhiStaticResource::get(key);
        }

        static bool getBuiltinShader(const std::string& name, Data& dst) {
            std::unordered_set<std::string> builtinShaders = {
                "chromatic", "circleBlur", "fisheye",
                "glitch", "grayscale", "noise",
                "pixel", "radialBlur", "shockwave", "vignette"
            };

            if (builtinShaders.contains(name)) {
                auto key = std::string("/shaders/") + name + ".glsl";
                dst = PhiStaticResource::get(key);
                return true;
            }

            return false;
        }

        static Data getFontData() {
            return PhiStaticResource::get("/font.ttf");
        }

        #ifdef EASY_PHI_TEXT_RENDERER
        static GL::TextManager::Renderer createTextRenderer() {
            auto tr = TextRenderer::Make();
            tr->loadFont(getFontData());
            return [tr](const std::string& text, ep_u64 fontSize) -> DecodedRGBATexture {
                return tr->render(text, fontSize);
            };
        }
        #endif
    };
}
#endif // EASY_PHI_PHI_RESOURCE

#ifdef EASY_PHI_MIL_RESOURCE
namespace easy_phi {
    #include "helpers/resources/milthm.cpp"

    struct MilStaticResourceHelpers {
        static MilTakeOverer::LineHeadTextureLoaderResult lineHeadTextureLoader() {
            return {
                .encoded = MilStaticResource::get("/line_head.png"),
                .connectPoint = 443.0
            };
        }
    };
}
#endif // EASY_PHI_MIL_RESOURCE

#endif // EASY_PHI_HPP
