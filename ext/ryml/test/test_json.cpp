#ifndef RYML_SINGLE_HEADER
#include "c4/yml/std/std.hpp"
#include "c4/yml/parse.hpp"
#include "c4/yml/emit.hpp"
#include <c4/format.hpp>
#include <c4/yml/detail/checks.hpp>
#include <c4/yml/detail/print.hpp>
#endif

#include "./test_case.hpp"

#include <gtest/gtest.h>

namespace foo {

template<class T>
struct vec2
{
    T x, y;
};
template<class T>
struct vec3
{
    T x, y, z;
};
template<class T>
struct vec4
{
    T x, y, z, w;
};

template<class T> size_t to_chars(c4::substr buf, vec2<T> v) { return c4::format(buf, "({},{})", v.x, v.y); }
template<class T> size_t to_chars(c4::substr buf, vec3<T> v) { return c4::format(buf, "({},{},{})", v.x, v.y, v.z); }
template<class T> size_t to_chars(c4::substr buf, vec4<T> v) { return c4::format(buf, "({},{},{},{})", v.x, v.y, v.z, v.w); }

template<class T> bool from_chars(c4::csubstr buf, vec2<T> *v) { size_t ret = c4::unformat(buf, "({},{})", v->x, v->y); return ret != c4::yml::npos; }
template<class T> bool from_chars(c4::csubstr buf, vec3<T> *v) { size_t ret = c4::unformat(buf, "({},{},{})", v->x, v->y, v->z); return ret != c4::yml::npos; }
template<class T> bool from_chars(c4::csubstr buf, vec4<T> *v) { size_t ret = c4::unformat(buf, "({},{},{},{})", v->x, v->y, v->z, v->w); return ret != c4::yml::npos; }

TEST(serialize, type_as_str)
{
    c4::yml::Tree t;

    auto r = t.rootref();
    r |= c4::yml::MAP;

    vec2<int> v2in{10, 11};
    vec2<int> v2out;
    r["v2"] << v2in;
    r["v2"] >> v2out;
    EXPECT_EQ(v2in.x, v2out.x);
    EXPECT_EQ(v2in.y, v2out.y);

    vec3<int> v3in{100, 101, 102};
    vec3<int> v3out;
    r["v3"] << v3in;
    r["v3"] >> v3out;
    EXPECT_EQ(v3in.x, v3out.x);
    EXPECT_EQ(v3in.y, v3out.y);
    EXPECT_EQ(v3in.z, v3out.z);

    vec4<int> v4in{1000, 1001, 1002, 1003};
    vec4<int> v4out;
    r["v4"] << v4in;
    r["v4"] >> v4out;
    EXPECT_EQ(v4in.x, v4out.x);
    EXPECT_EQ(v4in.y, v4out.y);
    EXPECT_EQ(v4in.z, v4out.z);
    EXPECT_EQ(v4in.w, v4out.w);

    char buf[256];
    c4::csubstr interm = c4::yml::emit_json(t, buf);
    EXPECT_EQ(interm, R"_({"v2": "(10,11)","v3": "(100,101,102)","v4": "(1000,1001,1002,1003)"})_");
}
} // namespace foo

namespace c4 {
namespace yml {

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

TEST(general, emitting)
{
    std::string cmpbuf;
    std::string cmpbuf2;

    Tree tree;
    auto r = tree.rootref();

    r |= MAP;  // this is needed to make the root a map

    r["foo"] = "1"; // ryml works only with strings.
    // Note that the tree will be __pointing__ at the
    // strings "foo" and "1" used here. You need
    // to make sure they have at least the same
    // lifetime as the tree.

    auto s = r["seq"]; // does not change the tree until s is written to.
    s |= SEQ;
    r["seq"].append_child() = "bar0"; // value of this child is now __pointing__ at "bar0"
    r["seq"].append_child() = "bar1";
    r["seq"].append_child() = "bar2";

    //print_tree(tree);

    // emit to stdout (can also emit to FILE* or ryml::span)
    emitrs_json(tree, &cmpbuf);
    EXPECT_EQ(cmpbuf, R"({"foo": 1,"seq": ["bar0","bar1","bar2"]})");

    // serializing: using operator<< instead of operator=
    // will make the tree serialize the value into a char
    // arena inside the tree. This arena can be reserved at will.
    int ch3 = 33, ch4 = 44;
    s.append_child() << ch3;
    s.append_child() << ch4;

    {
        std::string tmp = "child5";
        s.append_child() << tmp;
        // now tmp can go safely out of scope, as it was
        // serialized to the tree's internal string arena
    }

    emitrs_json(tree, &cmpbuf);
    EXPECT_EQ(cmpbuf, R"({"foo": 1,"seq": ["bar0","bar1","bar2",33,44,"child5"]})");

    // to serialize keys:
    int k = 66;
    r.append_child() << key(k) << 7;
    emitrs_json(tree, &cmpbuf);
    EXPECT_EQ(cmpbuf, R"({"foo": 1,"seq": ["bar0","bar1","bar2",33,44,"child5"],"66": 7})");
}

TEST(general, map_to_root)
{
    std::string cmpbuf; const char *exp;
    std::map<std::string, int> m({{"bar", 2}, {"foo", 1}});
    Tree t;
    t.rootref() << m;

    emitrs_json(t, &cmpbuf);
    exp = "{\"bar\": 2,\"foo\": 1}";
    EXPECT_EQ(cmpbuf, exp);

    t["foo"] << 10;
    t["bar"] << 20;

    m.clear();
    t.rootref() >> m;

    EXPECT_EQ(m["foo"], 10);
    EXPECT_EQ(m["bar"], 20);
}

TEST(general, json_stream_operator)
{
    std::map<std::string, int> out, m({{"bar", 2}, {"foo", 1}, {"foobar_barfoo:barfoo_foobar", 1001}, {"asdfjkl;", 42}, {"00000000000000000000000000000000000000000000000000000000000000", 1}});
    Tree t;
    t.rootref() << m;
    std::string str;
    {
        std::stringstream ss;
        ss << as_json(t);
        str = ss.str();
    }
    Tree res = c4::yml::parse_in_place(to_substr(str));
    EXPECT_EQ(res["foo"].val(), "1");
    EXPECT_EQ(res["bar"].val(), "2");
    EXPECT_EQ(res["foobar_barfoo:barfoo_foobar"].val(), "1001");
    EXPECT_EQ(res["asdfjkl;"].val(), "42");
    EXPECT_EQ(res["00000000000000000000000000000000000000000000000000000000000000"].val(), "1");
    res.rootref() >> out;
    EXPECT_EQ(out["foo"], 1);
    EXPECT_EQ(out["bar"], 2);
    EXPECT_EQ(out["foobar_barfoo:barfoo_foobar"], 1001);
    EXPECT_EQ(out["asdfjkl;"], 42);
    EXPECT_EQ(out["00000000000000000000000000000000000000000000000000000000000000"], 1);
}

TEST(emit_json, issue72)
{
    Tree t;
    NodeRef r = t.rootref();

    r |= MAP;
    r["1"] = "null";
    r["2"] = "true";
    r["3"] = "false";
    r["null"] = "1";
    r["true"] = "2";
    r["false"] = "3";

    std::string out;
    emitrs_json(t, &out);

    EXPECT_EQ(out, R"({"1": null,"2": true,"3": false,"null": 1,"true": 2,"false": 3})");
}


TEST(emit_json, issue121)
{
    Tree t = parse_in_arena(R"(
string_value: "string"
number_value: "9001"
broken_value: "0.30.2"
)");
    EXPECT_TRUE(t["string_value"].get()->m_type.type & VALQUO);
    EXPECT_TRUE(t["number_value"].get()->m_type.type & VALQUO);
    EXPECT_TRUE(t["broken_value"].get()->m_type.type & VALQUO);
    std::string out;
    emitrs_json(t, &out);
    EXPECT_EQ(out, R"({"string_value": "string","number_value": "9001","broken_value": "0.30.2"})");
    out.clear();
    emitrs(t, &out);
    EXPECT_EQ(out, R"(string_value: 'string'
number_value: '9001'
broken_value: '0.30.2'
)");
}

TEST(emit_json, issue291)
{
    Tree t = parse_in_arena("{}");
    t["james"] = "045";
    auto s = emitrs_json<std::string>(t);
    EXPECT_EQ(s, "{\"james\": \"045\"}");
}

TEST(emit_json, issue292)
{
    EXPECT_FALSE(csubstr("0.1.0").is_number());
    Tree t = parse_in_arena("{}");
    t["james"] = "1.2.3";
    auto s = emitrs_json<std::string>(t);
    EXPECT_EQ(s, "{\"james\": \"1.2.3\"}");
}

TEST(emit_json, issue297)
{
    char yml_buf[] = R"(
comment: |
   abc
   def
)";
    Tree t = parse_in_place(yml_buf);
    auto s = emitrs_json<std::string>(t);
    EXPECT_EQ(s, "{\"comment\": \"abc\\ndef\\n\"}");
}

TEST(emit_json, issue297_escaped_chars)
{
    Tree t = parse_in_arena("{}");
    t["quote"] = "abc\"def";
    t["newline"] = "abc\ndef";
    t["tab"] = "abc\tdef";
    t["carriage"] = "abc\rdef";
    t["backslash"] = "abc\\def";
    t["backspace"] = "abc\bdef";
    t["formfeed"] = "abc\fdef";
    std::string expected = R"({"quote": "abc\"def","newline": "abc\ndef","tab": "abc\tdef","carriage": "abc\rdef","backslash": "abc\\def","backspace": "abc\bdef","formfeed": "abc\fdef"})";
    auto actual = emitrs_json<std::string>(t);
    EXPECT_EQ(actual, expected);
}


#define _test(actual_src, expected_src)                     \
    {                                                       \
        SCOPED_TRACE(__LINE__);                             \
        csubstr file = __FILE__ ":" C4_XQUOTE(__LINE__);    \
        Tree actual = parse_in_arena(file, actual_src);     \
        Tree expected = parse_in_arena(file, expected_src); \
        test_compare(actual, expected);                     \
    }


TEST(json, basic)
{
    _test("", "");
    _test("{}", "{}");
    _test(R"("a":"b")",
          R"("a": "b")");
    _test(R"('a':'b')",
          R"('a': 'b')");
    _test(R"({'a':'b'})",
          R"({'a': 'b'})");
    _test(R"({"a":"b"})",
          R"({"a": "b"})");

    _test(R"({"a":{"a":"b"}})",
          R"({"a": {"a": "b"}})");
    _test(R"({'a':{'a':'b'}})",
          R"({'a': {'a': 'b'}})");
}

TEST(json, github142)
{
    _test(R"({"A":"B}"})",
          R"({"A": "B}"})");
    _test(R"({"A":"{B"})",
          R"({"A": "{B"})");
    _test(R"({"A":"{B}"})",
          R"({"A": "{B}"})");
    _test(R"({  "A":"B}"  })",
          R"({  "A": "B}"  })");
    _test(R"({"A":["B]","[C","[D]"]})",
          R"({"A": ["B]","[C","[D]"]})");
    //_test(R"({"A":["B\"]","[\"C","\"[D]\""]})", // VS2019 chokes on this.
    //      R"({"A": ["B\"]","[\"C","\"[D]\""]})");

    _test(R"({'A':'B}'})",
          R"({'A': 'B}'})");
    _test(R"({'A':'{B'})",
          R"({'A': '{B'})");
    _test(R"({'A':'{B}'})",
          R"({'A': '{B}'})");
    _test(R"({  'A':'B}'  })",
          R"({  'A': 'B}'  })");
    _test(R"({'A':['B]','[C','[D]']})",
          R"({'A': ['B]','[C','[D]']})");
    _test(R"({'A':['B'']','[''C','''[D]''']})",
          R"({'A': ['B'']','[''C','''[D]''']})");
}

TEST(json, github52)
{
    _test(R"({"a": "b","c": 42,"d": "e"})",
          R"({"a": "b","c": 42,"d": "e"})");
    _test(R"({"aaaa": "bbbb","cccc": 424242,"dddddd": "eeeeeee"})",
          R"({"aaaa": "bbbb","cccc": 424242,"dddddd": "eeeeeee"})");

    _test(R"({"a":"b","c":42,"d":"e"})",
          R"({"a": "b","c": 42,"d": "e"})");
    _test(R"({"aaaaa":"bbbbb","ccccc":424242,"ddddd":"eeeee"})",
          R"({"aaaaa": "bbbbb","ccccc": 424242,"ddddd": "eeeee"})");
    _test(R"({"a":"b","c":{},"d":"e"})",
          R"({"a": "b","c": {},"d": "e"})");
    _test(R"({"aaaaa":"bbbbb","ccccc":{    },"ddddd":"eeeee"})",
          R"({"aaaaa": "bbbbb","ccccc": {    },"ddddd": "eeeee"})");
    _test(R"({"a":"b","c":true,"d":"e"})",
          R"({"a": "b","c": true,"d": "e"})");
    _test(R"({"a":"b","c":false,"d":"e"})",
          R"({"a": "b","c": false,"d": "e"})");
    _test(R"({"a":"b","c":true,"d":"e"})",
          R"({"a": "b","c": true,"d": "e"})");
    _test(R"({"a":"b","c":null,"d":"e"})",
          R"({"a": "b","c": null,"d": "e"})");
    _test(R"({"aaaaa":"bbbbb","ccccc":false,"ddddd":"eeeee"})",
          R"({"aaaaa": "bbbbb","ccccc": false,"ddddd": "eeeee"})");
    _test(R"({"a":"b","c":false,"d":"e"})",
          R"({"a": "b","c": false,"d": "e"})");
    _test(R"({"aaaaa":"bbbbb","ccccc":true,"ddddd":"eeeee"})",
          R"({"aaaaa": "bbbbb","ccccc": true,"ddddd": "eeeee"})");
}

TEST(json, nested)
{
    _test(R"({"a":"b","c":{"a":"b","c":{},"d":"e"},"d":"e"})",
          R"({"a": "b","c": {"a": "b","c": {},"d": "e"},"d": "e"})");
    _test(R"({"a":"b","c":{"a":"b","c":{"a":"b","c":{},"d":"e"},"d":"e"},"d":"e"})",
          R"({"a": "b","c": {"a": "b","c": {"a": "b","c": {},"d": "e"},"d": "e"},"d": "e"})");
    _test(R"({"a":"b","c":{"a":"b","c":{"a":"b","c":{"a":"b","c":{},"d":"e"},"d":"e"},"d":"e"},"d":"e"})",
          R"({"a": "b","c": {"a": "b","c": {"a": "b","c": {"a": "b","c": {},"d": "e"},"d": "e"},"d": "e"},"d": "e"})");
    _test(R"({"a":"b","c":{"a":"b","c":{"a":"b","c":{"a":"b","c":{"a":"b","c":{},"d":"e"},"d":"e"},"d":"e"},"d":"e"},"d":"e"})",
          R"({"a": "b","c": {"a": "b","c": {"a": "b","c": {"a": "b","c": {"a": "b","c": {},"d": "e"},"d": "e"},"d": "e"},"d": "e"},"d": "e"})");

    _test(R"({"a":"b","c":["a","c","d","e"],"d":"e"})",
          R"({"a": "b","c": ["a","c","d","e"],"d": "e"})");
}

TEST(json, nested_end)
{
    _test(R"({"a":"b","d":"e","c":{"a":"b","d":"e","c":{}}})",
          R"({"a": "b","d": "e","c": {"a": "b","d": "e","c": {}}})");
    _test(R"({"a":"b","d":"e","c":{"a":"b","d":"e","c":{"a":"b","d":"e","c":{}}}})",
          R"({"a": "b","d": "e","c": {"a": "b","d": "e","c": {"a": "b","d": "e","c": {}}}})");
    _test(R"({"a":"b","d":"e","c":{"a":"b","d":"e","c":{"a":"b","d":"e","c":{"a":"b","d":"e","c":{}}}}})",
          R"({"a": "b","d": "e","c": {"a": "b","d": "e","c": {"a": "b","d": "e","c": {"a": "b","d": "e","c": {}}}}})");
    _test(R"({"a":"b","d":"e","c":{"a":"b","d":"e","c":{"a":"b","d":"e","c":{"a":"b","d":"e","c":{"a":"b","d":"e","c":{}}}}}})",
          R"({"a": "b","d": "e","c": {"a": "b","d": "e","c": {"a": "b","d": "e","c": {"a": "b","d": "e","c": {"a": "b","d": "e","c": {}}}}}})");
}

#undef _test


//-------------------------------------------
// this is needed to use the test case library
Case const* get_case(csubstr /*name*/)
{
    return nullptr;
}

} // namespace yml
} // namespace c4
