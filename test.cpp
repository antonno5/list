#include "tiny_test.hpp"
#include "stackallocator.h"

#include <algorithm>
#include <new>
#include <tuple>
#include <type_traits>
#include <vector>
#include <iterator>
#include <random>
#include <compare>
#include <list>


using testing::make_test;
using testing::SimpleTest;
using testing::PrettyTest;
using testing::TestGroup;
using groups_t = std::vector<TestGroup>;


template<typename T>
struct Iotaterator {
    T current;

    const T& operator*() const {
        return current;
    }

    const T* operator->() const {
        return &current;
    }

    Iotaterator& operator++() {
        ++current;
        return *this;
    }

    Iotaterator operator++(int) {
        auto copy = current;
        ++current;
        return {copy};
    }

    auto operator<=>(const Iotaterator&) const = default;
};

struct CustomAllocatorException {
    const char * const m_message;
    CustomAllocatorException(const char * const message) : m_message(message) {}
    const char * what() {
        return m_message;
    }

};

template<typename T>
struct ExceptionalAllocator {
    using value_type = T;

    size_t m_time_to_exception;
    std::allocator<T> m_actual_allocator;

    ExceptionalAllocator(size_t time) : m_time_to_exception(time) {}
    template<typename U>
    ExceptionalAllocator(const ExceptionalAllocator<U>& other) 
        : m_time_to_exception(other.m_time_to_exception) {}

    T* allocate(size_t n) {
        if (m_time_to_exception == 0) {
            throw CustomAllocatorException("this is exceptional");
        }
        if (m_time_to_exception < n) {
            m_time_to_exception = 0;
        } else {
            m_time_to_exception -= n;
        }
        return m_actual_allocator.allocate(n);
    }

    void deallocate(T* pointer, size_t n) {
        return m_actual_allocator.deallocate(pointer, n);
    }

    bool operator==(const ExceptionalAllocator& other) const = default;
};

struct Fragile {
    Fragile(int durability, int data): durability(durability), data(data) {}
    ~Fragile() = default;
    
    // for std::swap
    Fragile(Fragile&& other): Fragile() {
        *this = other;
    }

    Fragile(const Fragile& other): Fragile() {
        *this = other;
    }

    Fragile& operator=(const Fragile& other) {
        durability = other.durability - 1;
        data = other.data;
        if (durability <= 0) {
            throw 2;
        }
        return *this;
    }

    Fragile& operator=(Fragile&& other) {
        return *this = other;
    }

    int durability = -1;
    int data = -1;
private:
    Fragile() {

    }
};

struct Explosive {
    struct Safeguard {};

    inline static bool exploded = false;

    Explosive(): should_explode(true) {
        throw 1;
    }

    Explosive(Safeguard): should_explode(false) {

    }

    Explosive(const Explosive&): should_explode(true) {
        throw 2;
    }

    //TODO: is this ok..?
    Explosive& operator=(const Explosive&) {return *this;}

    ~Explosive() {
        exploded |= should_explode;
    }

private:
    const bool should_explode;
};

struct DefaultConstructible {
    DefaultConstructible() : data(default_data) { }

    int data = default_data;
    inline static const int default_data = 117;
};

struct NotDefaultConstructible {
    NotDefaultConstructible() = delete;
    NotDefaultConstructible(int input): data(input) {}
    int data;

    auto operator<=>(const NotDefaultConstructible&) const = default;
};

struct CountedException : public std::exception {

};

template<int WhenThrow>
struct Counted {
    inline static int counter = 0;

    Counted() {
        ++counter;
        if (counter == WhenThrow) {
            --counter;
            throw CountedException();
        }
    }

    Counted(const Counted&): Counted() { }

    ~Counted() {
        --counter;
    }
};

const size_t small_size = 17;
const size_t medium_size = 100;
const size_t big_size = 10'000;
const int nontrivial_int = 14;

template<typename iter, typename T>
struct CheckIter{
    using traits = std::iterator_traits<iter>;

    static_assert(std::is_same_v<std::remove_cv_t<typename traits::value_type>, std::remove_cv_t<T>>);
    static_assert(std::is_same_v<typename traits::pointer, T*>);
    static_assert(std::is_same_v<typename traits::reference, T&>);
    static_assert(std::is_same_v<typename traits::iterator_category, std::random_access_iterator_tag>);

    static_assert(std::is_same_v<decltype(std::declval<iter>()++), iter>);
    static_assert(std::is_same_v<decltype(++std::declval<iter>()), iter&>);
    static_assert(std::is_same_v<decltype(std::declval<iter>() + 1), iter>);
    static_assert(std::is_same_v<decltype(std::declval<iter>() += 1), iter&>);

    static_assert(std::is_same_v<decltype(std::declval<iter>() - std::declval<iter>()), typename traits::difference_type>);
    static_assert(std::is_same_v<decltype(*std::declval<iter>()), T&>);
    
    static_assert(std::is_same_v<decltype(std::declval<iter>() < std::declval<iter>()), bool>);
    static_assert(std::is_same_v<decltype(std::declval<iter>() <= std::declval<iter>()), bool>);
    static_assert(std::is_same_v<decltype(std::declval<iter>() > std::declval<iter>()), bool>);
    static_assert(std::is_same_v<decltype(std::declval<iter>() >= std::declval<iter>()), bool>);
    static_assert(std::is_same_v<decltype(std::declval<iter>() == std::declval<iter>()), bool>);
    static_assert(std::is_same_v<decltype(std::declval<iter>() != std::declval<iter>()), bool>);
};

TestGroup create_constructor_tests() {
    return { "constructors",
        make_test<PrettyTest>("default", [](auto& test){
            List<int> defaulted;
            test.check(defaulted.size() == 0);
            List<NotDefaultConstructible> without_default;
            test.check(without_default.size() == 0);
        }),
        make_test<PrettyTest>("copy", [](auto& test){
            List<NotDefaultConstructible> without_default;
            List<NotDefaultConstructible> copy = without_default;
            test.check(copy.size() == 0);
        }),
        make_test<PrettyTest>("with size", [](auto& test){
            size_t size = small_size;
            int value = nontrivial_int;
            List<int> simple(size);
            test.check((simple.size() == size_t(size)) && std::all_of(simple.begin(), simple.end(), [](int item){ return item == 0; }));
            List<NotDefaultConstructible> less_simple(size, value);
            test.check((less_simple.size() == size_t(size)) && std::all_of(less_simple.begin(), less_simple.end(), [&](const auto& item){ 
                        return item.data == value; 
            }));
            List<DefaultConstructible> default_constructor(size);
            test.check(std::all_of(default_constructor.begin(), default_constructor.end(), [](const auto& item) { 
                        return item.data == DefaultConstructible::default_data;
            }));
        }),
        make_test<PrettyTest>("assignment", [](auto& test){
            List<int> first(small_size, nontrivial_int);
            const auto second_size = small_size - 1;
            List<int> second(small_size - 1, nontrivial_int - 1);
            first = second;
            test.check((first.size() == second.size()) && (first.size() == second_size) && std::equal(first.begin(), first.end(), second.begin()));
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif
            second = second;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
            test.check((first.size() == second.size()) && (first.size() == second_size) && std::equal(first.begin(), first.end(), second.begin()));
        }),
        make_test<SimpleTest>("static asserts", []{
            using T1 = int;
            using T2 = NotDefaultConstructible;

            static_assert(std::is_default_constructible_v<List<T1>>, "should have default constructor");
            static_assert(std::is_default_constructible_v<List<T2>>, "should have default constructor");
            static_assert(std::is_copy_constructible_v<List<T1> >, "should have copy constructor");
            static_assert(std::is_copy_constructible_v<List<T2> >, "should have copy constructor");
            static_assert(std::is_constructible_v<List<T1>, int, const T1&>, "should have constructor from int and const T&");
            static_assert(std::is_constructible_v<List<T2>, int, const T2&>, "should have constructor from int and const T&");

            static_assert(std::is_copy_assignable_v<List<T1>>, "should have assignment operator");
            static_assert(std::is_copy_assignable_v<List<T2>>, "should have assignment operator");

            return true;       
        })
    };
}

TestGroup create_modification_tests() {
    return { "modification",
        make_test<PrettyTest>("exceptions", [](auto& test) {
            try {
                List<Counted<small_size>> big_list(medium_size);
            } catch (CountedException& e) {
                test.check(Counted<small_size>::counter == 0);
            }
            
            try {
                List<Explosive> explosive_list(medium_size);
                test.fail();
            } catch (...) { }

            try {
                List<Explosive> explosive_list;
            } catch (...) {
                // no objects should have been created
                test.fail();
            }
            test.check(Explosive::exploded == false);

            try {
                List<Explosive> guarded;
                auto safe = Explosive(Explosive::Safeguard{});
                guarded.push_back(safe);
            } catch (...) {

            }
            // Destructor should not be called for an object
            // with no finihshed constructor
            // the only destructor called - safe explosive with the safeguard
            test.check(Explosive::exploded == false);
        })
    };
}

TestGroup create_allocator_tests() {
    return {"allocator",
        make_test<PrettyTest>("exceptional allocator", [](auto& test) {
            using data_t = size_t;
            using alloc = ExceptionalAllocator<data_t>;
            auto exceptional_list = List<data_t, alloc>(alloc(small_size));
            auto& list = exceptional_list;
            for (size_t i = 0; i < small_size; ++i) {
                list.push_back(i);
            }
            // exactly small_size allocations should have occured, no exceptions
            auto try_modify = [&](auto callback, size_t expected_size) {
                try {
                    callback();
                    test.fail();
                } catch(CustomAllocatorException& e) {
                    test.equals(list.size(), expected_size);
                    test.check(std::equal(list.begin(), list.end(), Iotaterator<size_t>{0}));
                }
            };

            try_modify([&]{ list.push_back({}); }, small_size);
            try_modify([&]{ list.push_front({}); }, small_size);
            try_modify([&]{
                auto iter = list.begin();
                for (size_t i = 0; i < small_size / 2; ++i) {
                    ++iter;
                }
                list.insert(iter, 0);
            }, small_size);

            while(list.size() != 0) {
                list.pop_back();
            }

            try_modify([&]{ list.push_back({}); }, 0);
            try_modify([&]{ list.push_front({}); }, 0);
        }),

        make_test<PrettyTest>("stackallocator", [](auto& test) {
            using data_t = size_t;
            using alloc = StackAllocator<data_t, big_size>;
            auto big_storage = StackStorage<big_size>();
            auto big_list = List<data_t, alloc>(alloc(big_storage));
            auto big_stl_list = std::list<data_t, alloc>(alloc(big_storage));
            for (size_t i = 0; i < medium_size; ++i) {
                big_list.push_back(i);
                big_stl_list.push_front(i);
            }
            std::reverse(big_list.begin(), big_list.end());
            test.check(std::equal(big_list.begin(), big_list.end(), big_stl_list.begin()));
            test.check(std::equal(big_list.rbegin(), big_list.rend(), Iotaterator<data_t>{data_t(0)}));
        }),

        make_test<PrettyTest>("Memory limits", [](auto& test){
            if constexpr (std::is_base_of_v<std::false_type, StackAllocator<int, 1>>) {
                std::cout << "Skipping this test, as proper StackAllocator isn't present\n";
                test.fail();
                return;
            }
            using data_t = size_t;
            const size_t nbytes = small_size * (sizeof(data_t) + sizeof(void*) + sizeof(void*));
            using alloc = StackAllocator<data_t, nbytes>;
            auto small_storage = StackStorage<nbytes>();
            auto small_list = List<data_t, alloc>(alloc(small_storage));
            for (size_t i = 0; i < small_size; ++i) {
                small_list.push_front(i);
            }
            try {
                small_list.push_back({});
                test.fail();
            } catch(std::bad_alloc& e) { }
            test.equals(small_list.size(), small_size);
            test.check(std::equal(small_list.rbegin(), small_list.rend(), Iotaterator<data_t>{0}));
            try {
                small_list.push_front({});
                test.fail();
            } catch(std::bad_alloc& e) { }
            test.equals(small_list.size(), small_size);
            test.check(std::equal(small_list.rbegin(), small_list.rend(), Iotaterator<data_t>{0}));
            // no allocations for empty list
            auto empty_list = List<data_t, alloc>(alloc(small_storage));
            try {
                auto new_list = List<data_t, alloc>(alloc(small_storage));
                new_list.push_back({});
                test.fail();
            } catch(std::bad_alloc& e) { }
            // All allocated data is still valid
            test.equals(small_list.size(), small_size);
            test.check(std::equal(small_list.rbegin(), small_list.rend(), Iotaterator<data_t>{0}));
        })
    };
}


int main() {
    groups_t groups {};
    groups.push_back(create_constructor_tests());
    groups.push_back(create_modification_tests());
    groups.push_back(create_allocator_tests());

    bool res = true;
    for (auto& group : groups) {
        res &= group.run();
    }

    return res ? 0 : 1;
}

