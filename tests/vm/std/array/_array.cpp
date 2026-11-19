#include "tests/vm/std/helpers.hpp"

using namespace bloop::test::std_lib;

TEST_CASE("std.array.create") {
    Expect("return std.array.create(3);", "[ undefined, undefined, undefined ]");
    Expect("return std.array.create(3, 7);", "[ 7, 7, 7 ]");
    Expect("return std.array.create(0);", "[  ]");
    Expect("return std.length(std.array.create(100000, 1));", "100000"); // big enough to skip the nursery
    ExpectError("return std.array.create(-1);", "can't create an array of -1 elements");
    ExpectError("return std.array.create(\"3\");", "argument 1 must be \"int\"");
}
TEST_CASE("std.array.push and pop") {
    Expect("let a = [1]; let n = std.array.push(a, 2); return [n, a];", "[ 2, [ 1, 2 ] ]");
    Expect("let a = []; std.array.push(a, 1, 2, 3); return a;", "[ 1, 2, 3 ]");
    Expect("let a = [1, 2]; let last = std.array.pop(a); return [last, a, std.length(a)];", "[ 2, [ 1 ], 1 ]");
    Expect("let a = []; return std.array.pop(a);", "undefined");
    Expect("let a = []; for (let i = 0; i < 1000; i++) std.array.push(a, i); return [std.length(a), a[0], a[999]];", "[ 1000, 0, 999 ]");
    // a grown array is still the same array everywhere
    Expect("let a = [1]; let alias = a; let holder = { list: a }; std.array.push(a, 2, 3, 4, 5, 6); return [alias[5], holder.list[4], alias === a];", "[ 6, 5, true ]");
}
TEST_CASE("std.array.insert and remove") {
    Expect("let a = [1, 3]; std.array.insert(a, 1, 2); return a;", "[ 1, 2, 3 ]");
    Expect("let a = [2]; std.array.insert(a, 0, 1); std.array.insert(a, 2, 3); return a;", "[ 1, 2, 3 ]");
    Expect("let a = [1, 2, 3]; let removed = std.array.remove(a, 1); return [removed, a];", "[ 2, [ 1, 3 ] ]");
    Expect("let a = [1]; std.array.remove(a, 0); return std.length(a);", "0");
    ExpectError("let a = [1, 2]; std.array.insert(a, 3, 0);", "index 3 is out of range");
    ExpectError("let a = [1, 2]; std.array.remove(a, 2);", "index 2 is out of range");
    ExpectError("let a = [1, 2]; std.array.remove(a, -1);", "index -1 is out of range");
}
TEST_CASE("std.array.slice and concat") {
    Expect("return std.array.slice([1, 2, 3, 4]);", "[ 1, 2, 3, 4 ]");
    Expect("return std.array.slice([1, 2, 3, 4], 1);", "[ 2, 3, 4 ]");
    Expect("return std.array.slice([1, 2, 3, 4], 1, 3);", "[ 2, 3 ]");
    Expect("return std.array.slice([1, 2, 3, 4], -2);", "[ 3, 4 ]");
    Expect("return std.array.slice([1, 2, 3, 4], 0, -1);", "[ 1, 2, 3 ]");
    Expect("return std.array.slice([1, 2, 3, 4], 3, 1);", "[  ]");
    Expect("return std.array.slice([1, 2, 3, 4], 10, 20);", "[  ]");
    Expect("let a = [1, 2]; let b = std.array.slice(a); b[0] = 100; return a;", "[ 1, 2 ]"); // a copy
    Expect("return std.array.concat([1, 2], [3]);", "[ 1, 2, 3 ]");
    Expect("return std.array.concat([], []);", "[  ]");
}
TEST_CASE("std.array.index_of and contains") {
    Expect("return [std.array.index_of([5, 6, 7], 6), std.array.index_of([5, 6, 7], 8)];", "[ 1, -1 ]");
    Expect("return std.array.index_of([\"a\", \"b\"], \"b\");", "1"); // strings compare by content
    Expect("return std.array.index_of([1, 2], 2.0);", "-1");          // strict equality: 2 !== 2.0
    Expect("let o = {}; return [std.array.index_of([{}, o], o), std.array.index_of([{}], {})];", "[ 1, -1 ]");
    Expect("return [std.array.contains([1, 2], 2), std.array.contains([], 1)];", "[ true, false ]");
}
TEST_CASE("std.array.reverse, sort and join") {
    Expect("let a = [1, 2, 3]; let r = std.array.reverse(a); return [a, r === a];", "[ [ 3, 2, 1 ], true ]");
    Expect("return std.array.sort([3, 1, 2]);", "[ 1, 2, 3 ]");
    Expect("return std.array.sort([2.5, -1, 10u, 3]);", "[ -1, 2.500000, 3, 10 ]");
    Expect("return std.array.sort([\"pear\", \"apple\", \"fig\"]);", "[ apple, fig, pear ]");
    Expect("return std.array.sort([]);", "[  ]");
    ExpectError("return std.array.sort([1, \"a\"]);", "only sorts arrays of numbers or arrays of strings");
    Expect("return std.array.join([1, \"b\", true]);", "1,b,true");
    Expect("return std.array.join([1, 2, 3], \" - \");", "1 - 2 - 3");
    Expect("return std.array.join([]);", "");
}
TEST_CASE("std.array functions only accept arrays") {
    ExpectError("return std.array.push(\"abc\", 1);", "argument 1 must be \"array\", but got \"string\"");
    ExpectError("return std.array.pop({});", "argument 1 must be \"array\", but got \"object\"");
}

TEST_CASE("std.array.push_front and pop_front") {
    Expect("let a = [3]; let n = std.array.push_front(a, 1, 2); return [n, a];", "[ 3, [ 1, 2, 3 ] ]");
    Expect("let a = [1, 2]; let first = std.array.pop_front(a); return [first, a];", "[ 1, [ 2 ] ]");
    Expect("return std.array.pop_front([]);", "undefined");
    Expect("let a = []; for (let i = 0; i < 100; i++) std.array.push_front(a, i); return [std.length(a), a[0], a[99]];", "[ 100, 99, 0 ]");
}
TEST_CASE("std.array.resize and fill") {
    Expect("let a = [1, 2]; let r = std.array.resize(a, 4); return [a, r === a];", "[ [ 1, 2, undefined, undefined ], true ]");
    Expect("let a = [1, 2]; std.array.resize(a, 4, 0); return a;", "[ 1, 2, 0, 0 ]");
    Expect("let a = [1, 2, 3]; std.array.resize(a, 1); return a;", "[ 1 ]");
    // shrinking and growing again doesn't bring the old values back
    Expect("let a = [1, 2, 3]; std.array.resize(a, 1); std.array.resize(a, 3); return a;", "[ 1, undefined, undefined ]");
    ExpectError("std.array.resize([1], -1);", "can't resize an array to -1 elements");
    Expect("return std.array.fill([1, 2, 3, 4], 0);", "[ 0, 0, 0, 0 ]");
    Expect("return std.array.fill([1, 2, 3, 4], 0, 1, 3);", "[ 1, 0, 0, 4 ]");
    Expect("return std.array.fill([1, 2, 3, 4], 9, -1);", "[ 1, 2, 3, 9 ]");
}
TEST_CASE("std.array.transform") {
    Expect("return std.array.transform([1, 2, 3], (x) => x * 2);", "[ 2, 4, 6 ]");
    Expect("return std.array.transform([10, 20], (x, i) => x + i);", "[ 10, 21 ]");
    Expect("fn twice(x) { return x * 2; } return std.array.transform([1, 2], twice);", "[ 2, 4 ]");
    Expect("return std.array.transform([\"a\", \"b\"], std.string.to_upper);", "[ A, B ]"); // a native as the callback
    Expect("let a = [1, 2]; let b = std.array.transform(a, (x) => x); return b === a;", "false"); // a new array
    Expect("return std.array.transform([], (x) => x);", "[  ]");
    // callbacks can capture variables
    Expect("let factor = 3; return std.array.transform([1, 2], (x) => x * factor);", "[ 3, 6 ]");
}
TEST_CASE("std.array.find, find_last, find_index, find_last_index") {
    Expect("return std.array.find([1, 5, 8, 10], (x) => x > 4);", "5");
    Expect("return std.array.find_last([1, 5, 8, 10], (x) => x < 9);", "8");
    Expect("return std.array.find_index([1, 5, 8, 10], (x) => x > 4);", "1");
    Expect("return std.array.find_last_index([1, 5, 8, 10], (x) => x < 9);", "2");
    Expect("return [std.array.find([1], (x) => x > 4), std.array.find_last([], (x) => true)];", "[ undefined, undefined ]");
    Expect("return [std.array.find_index([1], (x) => x > 4), std.array.find_last_index([], (x) => true)];", "[ -1, -1 ]");
    Expect("return std.array.find_index([7, 8, 9], (x, i) => i === 2);", "2");
}
TEST_CASE("std.array.filter, all, any") {
    Expect("return std.array.filter([1, 2, 3, 4, 5], (x) => x % 2 === 1);", "[ 1, 3, 5 ]");
    Expect("return std.array.filter([1, 2], (x) => false);", "[  ]");
    Expect("return [std.array.all([2, 4], (x) => x % 2 === 0), std.array.all([2, 3], (x) => x % 2 === 0), std.array.all([], (x) => false)];", "[ true, false, true ]");
    Expect("return [std.array.any([1, 4], (x) => x % 2 === 0), std.array.any([1, 3], (x) => x % 2 === 0), std.array.any([], (x) => true)];", "[ true, false, false ]");
    // all and any stop early
    Expect("let calls = 0; std.array.any([1, 2, 3], (x) => { calls++; return x === 1; }); return calls;", "1");
    Expect("let calls = 0; std.array.all([1, 2, 3], (x) => { calls++; return x === 5; }); return calls;", "1");
}
TEST_CASE("std.array.for_each and accumulate") {
    Expect("let sum = 0; std.array.for_each([1, 2, 3], (x) => { sum += x; }); return sum;", "6");
    Expect("let s = \"\"; std.array.for_each([\"a\", \"b\"], (x, i) => { s = s + x + std.string.from(i); }); return s;", "a0b1");
    Expect("return std.array.accumulate([1, 2, 3, 4], (acc, x) => acc + x, 0);", "10");
    Expect("return std.array.accumulate([1, 2, 3, 4], (acc, x) => acc * x);", "24"); // starts from the first element
    Expect("return std.array.accumulate([], (acc, x) => acc + x, 5);", "5");
    Expect("return std.array.accumulate([\"a\", \"b\"], (acc, x, i) => acc + x + std.string.from(i), \">\");", ">a0b1");
    ExpectError("return std.array.accumulate([], (acc, x) => acc + x);", "an empty array needs an initial value");
}
TEST_CASE("std.array.sort with a comparator") {
    Expect("return std.array.sort([3, 1, 2], (a, b) => b - a);", "[ 3, 2, 1 ]");
    Expect("return std.array.sort([{ n: 2 }, { n: 1 }], (a, b) => a.n - b.n)[0].n;", "1");
    // stable: equal elements keep their order
    Expect("let a = [{ k: 1, v: \"a\" }, { k: 0, v: \"b\" }, { k: 1, v: \"c\" }, { k: 0, v: \"d\" }];"
           "std.array.sort(a, (x, y) => x.k - y.k); return std.array.join(std.array.transform(a, (x) => x.v), \"\");", "bdac");
    // an inconsistent comparator gives some order, but never breaks anything
    Expect("let n = 0; let a = std.array.sort([5, 3, 8, 1, 9, 2], (x, y) => { n++; return n % 3 - 1; }); return std.length(a);", "6");
    ExpectError("std.array.sort([2, 1], (a, b) => \"less\");", "the comparator must return a number, but it returned \"string\"");
    ExpectError("let a = [2, 1, 3]; std.array.sort(a, (x, y) => { std.array.pop(a); return x - y; });", "the comparator changed the length of the array");
}
TEST_CASE("std.array callbacks: argument counts") {
    Expect("return std.array.transform([1], () => 7);", "[ 7 ]"); // callbacks may ignore all arguments
    ExpectError("return std.array.transform([1], (a, b, c) => a);", "passed 2 arguments, but expected 3");
    ExpectError("return std.array.transform([1], 5);", "argument 2 must be \"function\", but got \"int\"");
}
TEST_CASE("std.array callbacks may change the array") {
    // the length is taken at the start, elements that disappear aren't visited
    Expect("let a = [1, 2, 3, 4]; let seen = []; std.array.for_each(a, (x) => { std.array.push(seen, x); std.array.pop(a); }); return seen;", "[ 1, 2 ]");
    Expect("let a = [1, 2]; let seen = []; std.array.for_each(a, (x) => { std.array.push(seen, x); std.array.push(a, 0); }); return [seen, std.length(a)];", "[ [ 1, 2 ], 4 ]");
}
TEST_CASE("std.array callbacks and garbage collection") {
    Expect("let r = std.array.transform([1, 2, 3], (x) => { std.gc_minor(); std.gc_major(); return [x, \"s\" + std.string.from(x)]; }); return r;",
           "[ [ 1, s1 ], [ 2, s2 ], [ 3, s3 ] ]");
    Expect("let r = std.array.filter([\"a\" + \"1\", \"b\" + \"2\"], (x) => { std.gc_major(); return true; }); return r;", "[ a1, b2 ]");
    Expect("return std.array.accumulate([1, 2, 3], (acc, x) => { std.gc_minor(); return acc + std.string.from(x); }, \"\");", "123");
}
TEST_CASE("std.array callbacks and exceptions") {
    // caught inside the callback: fine
    Expect("return std.array.transform([1, 2], (x) => { try { throw x; } catch (e) { return e * 10; } });", "[ 10, 20 ]");
    // a throw can't leave the callback, not even into a try around the call: the script terminates
    ExpectError("try { std.array.for_each([1], (x) => { throw \"oops\"; }); } catch (e) { let y = e; } return 1;",
                "uncaught exception in a function called by a native function: oops");
    // runtime errors in callbacks terminate the script too
    ExpectError("std.array.transform([1], (x) => x.missing);", "is not an aggregate type");
}
TEST_CASE("std.array callbacks inside callbacks") {
    Expect("return std.array.transform([1, 2], (x) => std.array.transform([x, x], (y) => { try { throw y; } catch (e) { return e + 1; } }));",
           "[ [ 2, 2 ], [ 3, 3 ] ]");
    // a throw from a function called by the callback, caught in the callback
    Expect("fn fail(v) { throw v; } return std.array.transform([1, 2], (x) => { try { fail(x * 5); } catch (e) { return e; } });", "[ 5, 10 ]");
    // an inner callback's throw can't be caught by the outer callback either
    ExpectError("std.array.for_each([1], (x) => { try { std.array.for_each([2], (y) => { throw y; }); } catch (e) { let z = e; } });",
                "uncaught exception in a function called by a native function: 2");
}
