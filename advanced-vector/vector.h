#pragma once

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept : buffer_(std::move(other.buffer_)), capacity_(std::move(other.capacity_)) {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            buffer_ = std::move(rhs.buffer_);
            capacity_ = std::move(rhs.capacity_);
            rhs.buffer_ = nullptr;
            rhs.capacity_ = 0;
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_ + 0;
    }

    iterator end() noexcept {
        return (data_ + size_);
    }

    const_iterator begin() const noexcept {
        return data_ + 0;
    }

    const_iterator end() const noexcept {
        return (data_ + size_);
    }

    const_iterator cbegin() const noexcept {
        return begin();
    }

    const_iterator cend() const noexcept {
        return end();
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t num_pos = std::distance(cbegin(), pos);
        if (size_ == Capacity()) { // Вместимости не достаточно
            size_t ns;
            if (size_ > 0) {
                ns = size_ * 2;
            } else {
                ns = 1;
            }
            RawMemory<T> new_data(ns);
            // constexpr оператор if будет вычислен во время компиляции
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                new (new_data + num_pos) T(std::forward<Args>(args)...);
                std::uninitialized_move_n(data_.GetAddress(), num_pos, new_data.GetAddress());
                if (pos != cend()) {
                    std::uninitialized_move_n(data_ + num_pos, size_ - num_pos, new_data + num_pos + 1);
                }
            } else {
                new (new_data + num_pos) T(std::forward<Args>(args)...);
                std::uninitialized_copy_n(data_.GetAddress(), num_pos, new_data.GetAddress());
                if (pos != cend()) {
                    std::uninitialized_copy_n(data_ + num_pos, size_ - num_pos, new_data + num_pos + 1);
                }
            }
            data_.Swap(new_data);
            std::destroy_n(new_data.GetAddress(), size_);
        } else { // Достаточно вместимости
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                EmplaceWMove(num_pos, std::forward<Args>(args)...);
            } else {
                EmplaceWCopy(num_pos, std::forward<Args>(args)...);
            }
        }
        ++size_;
        return &data_[num_pos];
    }
    
    iterator Erase(const_iterator pos) noexcept /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        size_t num_pos = std::distance(cbegin(), pos);
        std::move(data_ + num_pos + 1, end(), data_ + num_pos);
        (end() - 1)->~T();
        --size_;
        return &data_[num_pos];
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(std::forward<const_iterator>(pos), std::forward<const T&>(value));
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(std::forward<const_iterator>(pos), std::forward<T&&>(value));
    }

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)  //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(Vector&& other) noexcept : data_(std::move(other.data_)), size_(std::move(other.size_)) {
        other.size_ = 0;
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)  //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    void Resize(size_t new_size) {
        if (size_ == new_size)
        {
            return;
        } else if (size_ > new_size) {
            for (size_t i = new_size; i < size_; i++)
            {
                std::destroy_at(&data_[i]);
            }
            size_ = new_size;
        } else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(&data_[size_], (new_size - size_));
            size_ = new_size;
        }
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        Emplace(cend(), std::forward<Args>(args)...);
        return data_[size_ - 1];
    }

    void PopBack() noexcept {
        std::destroy_at(&data_[size_ - 1]);
        --size_;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        // constexpr оператор if будет вычислен во время компиляции
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        data_.Swap(new_data);
        std::destroy_n(new_data.GetAddress(), size_);
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                /* Скопировать элементы из rhs, создав при необходимости новые
                   или удалив существующие */
                size_t current_elem = 0;
                if (size_ >= rhs.size_) {
                    for (; current_elem < rhs.size_; current_elem++)
                    {
                        data_[current_elem] = rhs[current_elem];
                    }
                    for (; current_elem < size_; current_elem++)
                    {
                        std::destroy_at(&data_[current_elem]);
                    }
                    size_ = rhs.size_;
                } else {
                    for (; current_elem < size_; current_elem++)
                    {
                        data_[current_elem] = rhs[current_elem];
                    }
                    for (; current_elem < rhs.size_; current_elem++)
                    {
                        new (data_ + current_elem) T(rhs[current_elem]);
                    }
                    size_ = rhs.size_;
                }
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        data_ = std::move(rhs.data_);
        size_ = std::move(rhs.size_);
        rhs.size_ = 0;
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    // Использовать только если capacity позволяет добавить элемент
    template <typename... Args>
    void EmplaceWMove(size_t num_pos, Args&&... args) {
        if (size_ > 0 && num_pos != size_) {
            T tmp_val(std::forward<Args>(args)...);
            new (data_ + size_) T(std::move(data_[size_ - 1]));
            
            std::move_backward(data_ + num_pos, data_ + (size_ - 1), data_ + size_);  
            data_[num_pos] = std::move(tmp_val);
        } else {
            new (data_ + num_pos) T(std::forward<Args>(args)...);
        }
    }

    // Использовать только если capacity позволяет добавить элемент
    template <typename... Args>
    void EmplaceWCopy(size_t num_pos, Args&&... args) {
        if (size_ > 0 && num_pos != size_) {
            T tmp_val(std::forward<Args>(args)...);
            new (data_ + size_) T(std::move(data_[size_ - 1]));
            
            std::copy_backward(data_ + num_pos, data_ + (size_ - 1), data_ + size_);  
            data_[num_pos] = std::move(tmp_val);
        } else {
            new (data_ + num_pos) T(std::forward<Args>(args)...);
        }
    }
};