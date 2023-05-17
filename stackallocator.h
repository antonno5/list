#include <iostream>
#include <memory>
#include <iterator>

template <size_t N>
class StackStorage {
private:

    char my_buffer[N];
    size_t my_used;

public:

    StackStorage() : my_buffer(), my_used(reinterpret_cast<size_t>(my_buffer)) {}

    char* allocate(size_t bytes, size_t alignment) {
        size_t shift = my_used % alignment;
        if (shift) {
            my_used += alignment - shift;
        }
        char* ptr = reinterpret_cast<char*>(my_used);
        my_used += bytes;
        if (my_used > reinterpret_cast<size_t>(my_buffer) + N) {
            my_used -= bytes;
            throw std::bad_alloc();
        }
        return ptr;
    }

};

template<typename T, size_t N>
class StackAllocator {
public:

    StackStorage<N>* my_storage = nullptr;

    using value_type = T;
    using propagate_on_container_copy_assignment = std::false_type;

    StackAllocator() = default;

    explicit StackAllocator(const StackStorage<N>& storage) : my_storage(const_cast<StackStorage<N>*>(&storage) ) {}

    template<typename U>
    StackAllocator(const StackAllocator<U, N>& other) : my_storage(other.my_storage) {}

    template <typename U>
    struct rebind { //NOLINT
        using other = StackAllocator<U, N>;
    };

    T* allocate(size_t n) {
        return reinterpret_cast<T*>(my_storage->allocate(n * sizeof(T), alignof(T)));
    }

    void deallocate(T*, size_t) {}

};

template<typename T, class Allocator = std::allocator<T>>
class List {
private:

    struct BaseNode {
    private:

        BaseNode* my_next = this;
        BaseNode* my_prev = this;

    public:

        friend List;

    };

    struct Node : BaseNode {
    private:

        T value = T();

    public:

        Node() = default;

        Node(const T& value) : BaseNode(), value(value) {}

        Node(const T& value, BaseNode* next, BaseNode* prev) : value(value)
        , BaseNode(next, prev) {}

        T& get_value() {
            return value;
        }

    };

    template<typename Value>
    struct BasicIterator {
    private:

        BaseNode* my_node = nullptr;

    public:

        friend List;

        BasicIterator() = default;

        explicit BasicIterator(BaseNode* my_node) : my_node(my_node) {}

        using difference_type = ptrdiff_t;
        using value_type = Value;
        using reference = value_type&;
        using pointer = value_type*;
        using iterator_category = std::bidirectional_iterator_tag;

        BasicIterator& operator++() {
            my_node = my_node->my_next;
            return *this;
        }

        BasicIterator operator++(int) {
            auto copy = *this;
            ++*this;
            return copy;
        }

        BasicIterator& operator--() {
            my_node = my_node->my_prev;
            return *this;
        }

        BasicIterator operator--(int) {
            auto copy = *this;
            --*this;
            return copy;
        }

        reference operator*() const {
            auto node = reinterpret_cast<Node*>(my_node);
            return node->get_value();
        }

        pointer operator->() const {
            auto node = reinterpret_cast<Node*>(my_node);
            return &node->get_value();
        }

        bool operator==(const BasicIterator<Value>& that) const {
            return my_node == that.my_node;
        }

        bool operator!=(const BasicIterator<Value>& that) const {
            return !(my_node == that.my_node);
        }

        operator BasicIterator<const Value>() const {
            return BasicIterator<const Value>(my_node);
        }

    };

public:

    using value_type = T;
    using reference = T&;
    using pointer = T*;
    using iterator = BasicIterator<T>;
    using const_iterator = BasicIterator<const T>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

private:

    size_t my_size = 0;

    BaseNode my_end = BaseNode();

    iterator it_end = iterator(&my_end);

    Allocator my_allocator;

    using NodeAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<Node>;
    using NodeTraits = std::allocator_traits<NodeAlloc>;

    NodeAlloc my_node_allocator;

    void swap(List& that) {
        auto prev = my_end.my_prev, next = my_end.my_next;

        prev->my_next = &that.my_end;
        next->my_prev = &that.my_end;

        prev = that.my_end.my_prev, next = that.my_end.my_next;

        prev->my_next = &my_end;
        next->my_prev = &my_end;

        std::swap(my_allocator, that.my_allocator);
        std::swap(my_node_allocator, that.my_node_allocator);
        std::swap(my_end, that.my_end);
        std::swap(my_size, that.my_size);
    }

    void copy_from_other(const List& that) {
        try {
            for (iterator it = std::next(that.it_end); it != that.it_end; ++it) {
                push_back(*it);
            }
        } catch (...) {
            while (my_size) {
                pop_back();
            }
            throw;
        }
    }

public:

    List(const Allocator& alloc = Allocator()) : my_allocator(alloc), my_node_allocator(alloc) {}

    List(size_t new_size, const Allocator& alloc = Allocator()) : my_allocator(alloc), my_node_allocator(alloc) {
        try {
            for (size_t i = 0; i < new_size; ++i) {
                push_back();
            }
        } catch (...) {
            while (my_size) {
                pop_back();
            }
            throw;
        }
    }

    List(size_t new_size, const T& value, const Allocator& alloc = Allocator()) : my_allocator(alloc), my_node_allocator(alloc) {
        try {
            for (size_t i = 0; i < new_size; ++i) {
                push_back(value);
            }
        } catch(...) {
            while (my_size) {
                pop_back();
            }
            throw;
        }
    }

    Allocator get_allocator() const {
        return my_allocator;
    }

    List(const List& that, const Allocator& allocator) : List(allocator){
        copy_from_other(that);
    }

    List(const List& that) : List(std::allocator_traits<Allocator>::select_on_container_copy_construction(that.my_allocator)) {
        copy_from_other(that);
    }

    List& operator=(const List& that) {
        if (this != &that) {
            List copy(that, std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value
                            ? that.my_allocator : my_allocator);
            this->swap(copy);
        }
        return *this;
    }

    size_t size() const {
        return my_size;
    }

    template<typename... Args>
    iterator insert(const_iterator it, const Args&... args) {
        Node *ptr = nullptr;

        try {
            ptr = NodeTraits::allocate(my_node_allocator, 1);
        } catch(...) {
            throw;
        }

        try {
            NodeTraits::construct(my_node_allocator, ptr, args...);
        } catch(...) {
            NodeTraits::deallocate(my_node_allocator, ptr, 1);
            throw;
        }

        BaseNode* base_ptr = static_cast<BaseNode*>(ptr);

        BaseNode* node = it.my_node;

        base_ptr->my_prev = node->my_prev;
        base_ptr->my_next = node;
        node->my_prev->my_next = base_ptr;
        node->my_prev = base_ptr;

        ++my_size;

        return iterator(base_ptr);
    }

    iterator erase(const_iterator it) {
        --my_size;

        BaseNode* node = it.my_node;

        BaseNode* prev = node->my_prev;
        BaseNode* next = node->my_next;

        prev->my_next = next;
        next->my_prev = prev;

        iterator it_next = iterator(node->my_next);

        NodeTraits::destroy(my_node_allocator, reinterpret_cast<Node*>(it.my_node));
        NodeTraits::deallocate(my_node_allocator, reinterpret_cast<Node*>(it.my_node), 1);

        return it_next;
    }

    iterator begin() {
        return std::next(it_end);
    }

    const_iterator cbegin() const {
        return const_iterator(std::next(it_end));
    }

    const_iterator begin() const {
        return cbegin();
    }

    iterator end() {
        return it_end;
    }

    const_iterator cend() const {
        return const_iterator(it_end);
    }

    const_iterator end() const {
        return cend();
    }

    reverse_iterator rbegin() {
        return std::make_reverse_iterator(it_end);
    }

    const_reverse_iterator crbegin() const {
        return std::make_reverse_iterator(cend());
    }

    const_reverse_iterator rbegin() const {
        return crbegin();
    }

    reverse_iterator rend() {
        return std::make_reverse_iterator(end());
    }

    const_reverse_iterator crend() const {
        return std::make_reverse_iterator(cend());
    }

    const_reverse_iterator rend() const {
        return crend();
    }

    iterator push_back(const T& that) {
        return insert(it_end, that);
    }

    iterator push_back() {
        return insert(it_end);
    }

    iterator push_front(const T& that) {
        return insert(begin(), that);
    }

    iterator push_front() {
        return insert(begin());
    }

    iterator pop_back() {
        return erase(std::prev(it_end));
    }

    iterator pop_front() {
        return erase(std::next(it_end));
    }

    ~List() {
        while (my_size) {
            pop_back();
        }
    }

};

