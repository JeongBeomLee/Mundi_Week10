#pragma once

/** UE5 스타일 기본 타입 정의 */
typedef int int32;
typedef unsigned int uint32;
typedef long long int64;
typedef unsigned long long uint64;
typedef char int8;
typedef unsigned char uint8;
typedef short int16;
typedef unsigned short uint16;
typedef float float32;
typedef double float64;
typedef bool UBOOL;
typedef wchar_t WIDECHAR;

/** UE5 스타일 문자열 정의 */
typedef std::string FString;
typedef std::wstring FWideString;

/** TPair 구현 */
template<typename T1, typename T2>
using TPair = std::pair<T1, T2>;

/** TStaticArray 구현 (고정 크기 배열) */
template<typename T, size_t N>
using TStaticArray = std::array<T, N>;

/** TArray 구현 */
template<typename T>
class TArray : public std::vector<T>
{
public:
    using std::vector<T>::vector; /** 생성자 상속 */

    /** 요소 추가 */
    int32 Add(const T& Item)
    {
        this->push_back(Item);
        return static_cast<int32>(this->size() - 1);
    }

    template<typename... Args>
    int32 Emplace(Args&&... args)
    {
        this->emplace_back(std::forward<Args>(args)...);
        return static_cast<int32>(this->size() - 1);
    }

    /** 고유 요소만 추가 */
    int32 AddUnique(const T& Item)
    {
        auto it = std::find(this->begin(), this->end(), Item);
        if (it == this->end())
        {
            return Add(Item);
        }
        return static_cast<int32>(std::distance(this->begin(), it));
    }

    /** 배열 병합 */
    void Append(const TArray<T>& Other)
    {
        this->insert(this->end(), Other.begin(), Other.end());
    }

    /** 삽입 */
    void Insert(const T& Item, int32 Index)
    {
        this->insert(this->begin() + Index, Item);
    }

    /** 제거 */
    void RemoveAt(int32 Index)
    {
        this->erase(this->begin() + Index);
    }

    bool Remove(const T& Item)
    {
        auto it = std::find(this->begin(), this->end(), Item);
        if (it != this->end())
        {
            this->erase(it);
            return true;
        }
        return false;
    }

    int32 RemoveAll(const T& Item)
    {
        auto oldSize = this->size();
        this->erase(std::remove(this->begin(), this->end(), Item), this->end());
        return static_cast<int32>(oldSize - this->size());
    }

    /** 크기 관련 */
    int32 Num() const
    {
        return static_cast<int32>(this->size());
    }

    bool IsEmpty() const
    {
        return this->empty();
    }

    void Empty()
    {
        this->clear();
    }

    // 보유 용량(capacity)을 실제 크기(Num)에 맞춰 축소
    void Shrink()
    {
        this->shrink_to_fit();
    }

    void Reserve(int64 Capacity)
    {
        this->reserve(static_cast<size_t>(Capacity));
    }

    void SetNum(int32 NewSize)
    {
        this->resize(static_cast<size_t>(NewSize));
    }

    void SetNum(int32 NewSize, const T& DefaultValue)
    {
        this->resize(static_cast<size_t>(NewSize), DefaultValue);
    }

    /** 접근 */
    T& Last()
    {
        return this->back();
    }

    const T& Last() const
    {
        return this->back();
    }

    /** 내부 데이터 포인터 반환 */
    T* GetData()
    {
        return this->data();
    }

    const T* GetData() const
    {
        return this->data();
    }

    /** Stack 기능 - std::stack 대체 */
    void Push(const T& Item)
    {
        this->push_back(Item);
    }

    T Pop()
    {
        T Item = this->back();
        this->pop_back();
        return Item;
    }

    /** 검색 */
    int32 Find(const T& Item) const
    {
        auto it = std::find(this->begin(), this->end(), Item);
        return (it != this->end()) ? static_cast<int32>(std::distance(this->begin(), it)) : -1;
    }

    bool Contains(const T& Item) const
    {
        return Find(Item) != -1;
    }

    /** 정렬 */
    void Sort()
    {
        std::sort(this->begin(), this->end());
    }

    template<typename Predicate>
    void Sort(Predicate Pred)
    {
        std::sort(this->begin(), this->end(), Pred);
    }
};

/** TSet - 해시 기반 집합 */
template<typename T>
class TSet : public std::unordered_set<T>
{
public:
    using std::unordered_set<T>::unordered_set;

    /** 요소 추가 */
    void Add(const T& Item)
    {
        this->insert(Item);
    }

    /** 제거 */
    bool Remove(const T& Item)
    {
        return this->erase(Item) > 0;
    }

    /** 크기 관련 */
    int32 Num() const
    {
        return static_cast<int32>(this->size());
    }

    bool IsEmpty() const
    {
        return this->empty();
    }

    void Empty()
    {
        this->clear();
    }

    /** 검색 */
    bool Contains(const T& Item) const
    {
        return this->find(Item) != this->end();
    }

    /** 집합 연산 */
    TSet<T> Union(const TSet<T>& Other) const
    {
        TSet<T> Result = *this;
        for (const auto& Item : Other)
        {
            Result.Add(Item);
        }
        return Result;
    }

    TSet<T> Intersect(const TSet<T>& Other) const
    {
        TSet<T> Result;
        for (const auto& Item : *this)
        {
            if (Other.Contains(Item))
            {
                Result.Add(Item);
            }
        }
        return Result;
    }

    TSet<T> Difference(const TSet<T>& Other) const
    {
        TSet<T> Result;
        for (const auto& Item : *this)
        {
            if (!Other.Contains(Item))
            {
                Result.Add(Item);
            }
        }
        return Result;
    }

    /** 배열로 변환 */
    TArray<T> Array() const
    {
        TArray<T> Result;
        Result.Reserve(this->size());
        for (const auto& Item : *this)
        {
            Result.Add(Item);
        }
        return Result;
    }
};

/** TMap - 해시 기반 연관 컨테이너 */
template<typename KeyType, typename ValueType>
class TMap : public std::unordered_map<KeyType, ValueType>
{
public:
    using std::unordered_map<KeyType, ValueType>::unordered_map;

    /** 요소 추가/수정 */
    void Add(const KeyType& Key, const ValueType& Value)
    {
        (*this)[Key] = Value;
    }

    template<typename... Args>
    void Emplace(const KeyType& Key, Args&&... args)
    {
        this->emplace(Key, ValueType(std::forward<Args>(args)...));
    }

    /** 제거 */
    bool Remove(const KeyType& Key)
    {
        return this->erase(Key) > 0;
    }

    /** 크기 관련 */
    int32 Num() const
    {
        return static_cast<int32>(this->size());
    }

    bool IsEmpty() const
    {
        return this->empty();
    }

    void Empty()
    {
        this->clear();
    }

    /** 검색 */
    bool Contains(const KeyType& Key) const
    {
        return this->find(Key) != this->end();
    }

    ValueType* Find(const KeyType& Key)
    {
        auto it = this->find(Key);
        return (it != this->end()) ? &it->second : nullptr;
    }

    const ValueType* Find(const KeyType& Key) const
    {
        auto it = this->find(Key);
        return (it != this->end()) ? &it->second : nullptr;
    }

    /** 찾거나 기본값 반환 */
    ValueType FindRef(const KeyType& Key) const
    {
        auto it = this->find(Key);
        return (it != this->end()) ? it->second : ValueType{};
    }

    /** 키/값 배열 반환 */
    TArray<KeyType> GetKeys() const
    {
        TArray<KeyType> Keys;
        Keys.Reserve(this->size());
        for (const auto& Pair : *this)
        {
            Keys.Add(Pair.first);
        }
        return Keys;
    }

    TArray<ValueType> GetValues() const
    {
        TArray<ValueType> Values;
        Values.Reserve(this->size());
        for (const auto& Pair : *this)
        {
            Values.Add(Pair.second);
        }
        return Values;
    }
};

/** TLinkedList - 연결 리스트 (잘 사용하지 않음) */
template<typename T>
using TLinkedList = std::list<T>;

/** TDoubleLinkedList - 이중 연결 리스트 */
template<typename T>
using TDoubleLinkedList = std::list<T>;

/** 큐 모드 열거형 */
enum class EQueueMode
{
    Spsc,           /** Single Producer Single Consumer (기본 FIFO) */
    Mpmc,           /** Multiple Producer Multiple Consumer */
    Mpsc,           /** Multiple Producer Single Consumer */
    Spmc,           /** Single Producer Multiple Consumer */
    Priority        /** Priority Queue */
};

/** 기본 비교자 (std::less와 동일) */
template<typename T>
struct TDefaultCompare
{
    bool operator()(const T& a, const T& b) const
    {
        return a < b;
    }
};

/** 기본 TQueue - FIFO 큐 */
template<typename T, EQueueMode Mode = EQueueMode::Spsc, typename Compare = TDefaultCompare<T>>
class TQueue : public std::queue<T>
{
public:
    using std::queue<T>::queue;

    /** 요소 추가 */
    void Enqueue(const T& Item)
    {
        this->push(Item);
    }

    /** 요소 제거 */
    bool Dequeue(T& OutItem)
    {
        if (this->empty())
        {
            return false;
        }

        OutItem = this->front();
        this->pop();
        return true;
    }

    /** 맨 앞 요소 확인 */
    bool Peek(T& OutItem) const
    {
        if (this->empty())
        {
            return false;
        }

        OutItem = this->front();
        return true;
    }

    /** 크기 관련 */
    int32 Num() const
    {
        return static_cast<int32>(this->size());
    }

    bool IsEmpty() const
    {
        return this->empty();
    }

    void Empty()
    {
        *this = TQueue<T, Mode, Compare>();
    }
};

/** Priority Queue를 위한 특수화 - 기본 비교자 */
template<typename T>
class TQueue<T, EQueueMode::Priority, TDefaultCompare<T>> : public std::priority_queue<T>
{
public:
    using std::priority_queue<T>::priority_queue;

    void Enqueue(const T& Item)
    {
        this->push(Item);
    }

    bool Dequeue(T& OutItem)
    {
        if (this->empty())
        {
            return false;
        }

        OutItem = this->top();
        this->pop();
        return true;
    }

    bool Peek(T& OutItem) const
    {
        if (this->empty())
        {
            return false;
        }

        OutItem = this->top();
        return true;
    }

    int32 Num() const
    {
        return static_cast<int32>(this->size());
    }

    bool IsEmpty() const
    {
        return this->empty();
    }

    void Empty()
    {
        *this = TQueue<T, EQueueMode::Priority, TDefaultCompare<T>>();
    }
};

/** Priority Queue를 위한 특수화 - 커스텀 비교자 */
template<typename T, typename Compare>
class TQueue<T, EQueueMode::Priority, Compare> : public std::priority_queue<T, std::vector<T>, Compare>
{
public:
    using std::priority_queue<T, std::vector<T>, Compare>::priority_queue;

    void Enqueue(const T& Item)
    {
        this->push(Item);
    }

    bool Dequeue(T& OutItem)
    {
        if (this->empty())
        {
            return false;
        }

        OutItem = this->top();
        this->pop();
        return true;
    }

    bool Peek(T& OutItem) const
    {
        if (this->empty())
        {
            return false;
        }

        OutItem = this->top();
        return true;
    }

    int32 Num() const
    {
        return static_cast<int32>(this->size());
    }

    bool IsEmpty() const
    {
        return this->empty();
    }

    void Empty()
    {
        *this = TQueue<T, EQueueMode::Priority, Compare>();
    }
};

/** 편의성을 위한 매크로들 */
#define TPriorityQueue(T) TQueue<T, EQueueMode::Priority>
#define TPriorityQueueWithCompare(T, Compare) TQueue<T, EQueueMode::Priority, Compare>


// ANSI 문자열을 UTF-8로 변환하는 유틸리티 함수
// TODO (동민, 한글) - 혹시나 프로젝트 설정의 /utf-8 옵션을 끈다면 이 설정이 무의미해집니다.
static inline FString ToUtf8(const FString& Ansi)
{
    if (Ansi.empty()) return {};

    // ANSI -> Wide
    int WideLen = MultiByteToWideChar(CP_ACP, 0, Ansi.c_str(), -1, nullptr, 0);
    FWideString Wide(static_cast<size_t>(WideLen - 1), L'\0');
    MultiByteToWideChar(CP_ACP, 0, Ansi.c_str(), -1, Wide.data(), WideLen);

    // Wide -> UTF-8
    int Utf8Len = WideCharToMultiByte(CP_UTF8, 0, Wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    FString Utf8(static_cast<size_t>(Utf8Len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, Wide.c_str(), -1, Utf8.data(), Utf8Len, nullptr, nullptr);
    return Utf8;
}