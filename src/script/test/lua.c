/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/test/suite.h"

#include <mgba/internal/script/lua.h>
#include <mgba/script/macros.h>

#define SETUP_LUA \
	struct mScriptContext context; \
	mScriptContextInit(&context); \
	struct mScriptEngineContext* lua = mScriptContextRegisterEngine(&context, mSCRIPT_ENGINE_LUA)

#define LOAD_PROGRAM(PROG) \
	do { \
		struct VFile* vf = VFileFromConstMemory(PROG, strlen(PROG)); \
		assert_true(lua->load(lua, NULL, vf)); \
		vf->close(vf); \
	} while(0)

#define TEST_PROGRAM(PROG) \
	LOAD_PROGRAM(PROG); \
	assert_true(lua->run(lua)); \

struct Test {
	int32_t i;
	int32_t (*ifn0)(struct Test*);
	int32_t (*ifn1)(struct Test*, int);
	void (*vfn0)(struct Test*);
	void (*vfn1)(struct Test*, int);
	int32_t (*icfn0)(const struct Test*);
};

static int identityInt(int in) {
	return in;
}

static int addInts(int a, int b) {
	return a + b;
}

static int32_t testI0(struct Test* a) {
	return a->i;
}

static int32_t testI1(struct Test* a, int b) {
	return a->i + b;
}

static int32_t testIC0(const struct Test* a) {
	return a->i;
}

static void testV0(struct Test* a) {
	++a->i;
}

static void testV1(struct Test* a, int b) {
	a->i += b;
}

static int32_t sum(struct mScriptList* list) {
	int32_t sum = 0;
	size_t i;
	for (i = 0; i < mScriptListSize(list); ++i) {
		struct mScriptValue value;
		if (!mScriptCast(mSCRIPT_TYPE_MS_S32, mScriptListGetPointer(list, i), &value)) {
			continue;
		}
		sum += value.value.s32;
	}
	return sum;
}

mSCRIPT_BIND_FUNCTION(boundIdentityInt, S32, identityInt, 1, S32, a);
mSCRIPT_BIND_FUNCTION(boundAddInts, S32, addInts, 2, S32, a, S32, b);
mSCRIPT_BIND_FUNCTION(boundSum, S32, sum, 1, LIST, list);

mSCRIPT_DECLARE_STRUCT(Test);
mSCRIPT_DECLARE_STRUCT_D_METHOD(Test, S32, ifn0, 0);
mSCRIPT_DECLARE_STRUCT_D_METHOD(Test, S32, ifn1, 1, S32, b);
mSCRIPT_DECLARE_STRUCT_CD_METHOD(Test, S32, icfn0, 0);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(Test, vfn0, 0);
mSCRIPT_DECLARE_STRUCT_VOID_D_METHOD(Test, vfn1, 1, S32, b);
mSCRIPT_DECLARE_STRUCT_METHOD(Test, S32, i0, testI0, 0);
mSCRIPT_DECLARE_STRUCT_METHOD(Test, S32, i1, testI1, 1, S32, b);
mSCRIPT_DECLARE_STRUCT_C_METHOD(Test, S32, ic0, testIC0, 0);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(Test, v0, testV0, 0);
mSCRIPT_DECLARE_STRUCT_VOID_METHOD(Test, v1, testV1, 1, S32, b);

mSCRIPT_DEFINE_STRUCT(Test)
	mSCRIPT_DEFINE_STRUCT_MEMBER(Test, S32, i)
	mSCRIPT_DEFINE_STRUCT_METHOD(Test, ifn0)
	mSCRIPT_DEFINE_STRUCT_METHOD(Test, ifn1)
	mSCRIPT_DEFINE_STRUCT_METHOD(Test, icfn0)
	mSCRIPT_DEFINE_STRUCT_METHOD(Test, vfn0)
	mSCRIPT_DEFINE_STRUCT_METHOD(Test, vfn1)
	mSCRIPT_DEFINE_STRUCT_METHOD(Test, i0)
	mSCRIPT_DEFINE_STRUCT_METHOD(Test, i1)
	mSCRIPT_DEFINE_STRUCT_METHOD(Test, ic0)
	mSCRIPT_DEFINE_STRUCT_METHOD(Test, v0)
	mSCRIPT_DEFINE_STRUCT_METHOD(Test, v1)
mSCRIPT_DEFINE_END;

M_TEST_SUITE_SETUP(mScriptLua) {
	if (mSCRIPT_ENGINE_LUA->init) {
		mSCRIPT_ENGINE_LUA->init(mSCRIPT_ENGINE_LUA);
	}
	return 0;
}

M_TEST_SUITE_TEARDOWN(mScriptLua) {
	if (mSCRIPT_ENGINE_LUA->deinit) {
		mSCRIPT_ENGINE_LUA->deinit(mSCRIPT_ENGINE_LUA);
	}
	return 0;
}

M_TEST_DEFINE(create) {
	struct mScriptContext context;
	mScriptContextInit(&context);
	struct mScriptEngineContext* lua = mSCRIPT_ENGINE_LUA->create(mSCRIPT_ENGINE_LUA, &context);
	lua->destroy(lua);
	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(loadGood) {
	struct mScriptContext context;
	mScriptContextInit(&context);
	struct mScriptEngineContext* lua = mSCRIPT_ENGINE_LUA->create(mSCRIPT_ENGINE_LUA, &context);

	const char* program = "-- test\n";
	struct VFile* vf = VFileFromConstMemory(program, strlen(program));
	assert_true(lua->load(lua, NULL, vf));

	lua->destroy(lua);
	mScriptContextDeinit(&context);
	vf->close(vf);
}

M_TEST_DEFINE(loadBadSyntax) {
	struct mScriptContext context;
	mScriptContextInit(&context);
	struct mScriptEngineContext* lua = mSCRIPT_ENGINE_LUA->create(mSCRIPT_ENGINE_LUA, &context);

	const char* program = "Invalid syntax! )\n";
	struct VFile* vf = VFileFromConstMemory(program, strlen(program));
	assert_false(lua->load(lua, NULL, vf));

	lua->destroy(lua);
	mScriptContextDeinit(&context);
	vf->close(vf);
}

M_TEST_DEFINE(runNop) {
	SETUP_LUA;

	LOAD_PROGRAM("return");
	assert_true(lua->run(lua));

	// Make sure we can run it twice
	assert_true(lua->run(lua));

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(getGlobal) {
	SETUP_LUA;

	struct mScriptValue a = mSCRIPT_MAKE_S32(1);
	struct mScriptValue* val;

	TEST_PROGRAM("a = 1");

	val = lua->getGlobal(lua, "a");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);

	TEST_PROGRAM("b = 1");

	val = lua->getGlobal(lua, "a");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);

	val = lua->getGlobal(lua, "b");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);

	a = mSCRIPT_MAKE_S32(2);
	TEST_PROGRAM("a = 2");

	val = lua->getGlobal(lua, "a");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);

	a = mSCRIPT_MAKE_S32(3);
	TEST_PROGRAM("b = a + b");

	val = lua->getGlobal(lua, "b");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(setGlobal) {
	SETUP_LUA;

	struct mScriptValue a = mSCRIPT_MAKE_S32(1);
	struct mScriptValue* val;

	LOAD_PROGRAM("a = b");
	assert_true(lua->setGlobal(lua, "b", &a));

	val = lua->getGlobal(lua, "b");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);

	assert_true(lua->run(lua));

	val = lua->getGlobal(lua, "a");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	a = mSCRIPT_MAKE_S32(2);
	assert_false(a.type->equal(&a, val));
	mScriptValueDeref(val);

	val = lua->getGlobal(lua, "a");
	assert_non_null(val);
	assert_false(a.type->equal(&a, val));
	mScriptValueDeref(val);

	assert_true(lua->setGlobal(lua, "b", &a));

	val = lua->getGlobal(lua, "b");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);

	assert_true(lua->run(lua));

	val = lua->getGlobal(lua, "a");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);

	assert_true(lua->setGlobal(lua, "b", NULL));
	val = lua->getGlobal(lua, "b");
	assert_null(val);

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(callLuaFunc) {
	SETUP_LUA;

	struct mScriptValue* fn;

	TEST_PROGRAM("function a(b) return b + 1 end; function c(d, e) return d + e end");
	assert_null(lua->getError(lua));

	fn = lua->getGlobal(lua, "a");
	assert_non_null(fn);
	assert_int_equal(fn->type->base, mSCRIPT_TYPE_FUNCTION);

	struct mScriptFrame frame;
	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	assert_true(mScriptInvoke(fn, &frame));
	int64_t val;
	assert_true(mScriptPopS64(&frame.returnValues, &val));
	assert_int_equal(val, 2);

	mScriptFrameDeinit(&frame);
	mScriptValueDeref(fn);

	fn = lua->getGlobal(lua, "c");
	assert_non_null(fn);
	assert_int_equal(fn->type->base, mSCRIPT_TYPE_FUNCTION);

	mScriptFrameInit(&frame);
	mSCRIPT_PUSH(&frame.arguments, S32, 1);
	mSCRIPT_PUSH(&frame.arguments, S32, 2);
	assert_true(mScriptInvoke(fn, &frame));
	assert_true(mScriptPopS64(&frame.returnValues, &val));
	assert_int_equal(val, 3);

	mScriptFrameDeinit(&frame);
	mScriptValueDeref(fn);

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(callCFunc) {
	SETUP_LUA;

	struct mScriptValue a = mSCRIPT_MAKE_S32(1);
	struct mScriptValue* val;

	assert_true(lua->setGlobal(lua, "b", &boundIdentityInt));
	assert_true(lua->setGlobal(lua, "d", &boundAddInts));
	TEST_PROGRAM("a = b(1); c = d(1, 2)");
	assert_null(lua->getError(lua));

	val = lua->getGlobal(lua, "a");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);

	a = mSCRIPT_MAKE_S32(3);
	val = lua->getGlobal(lua, "c");
	assert_non_null(val);
	assert_true(a.type->equal(&a, val));
	mScriptValueDeref(val);

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(globalStructFieldGet) {
	SETUP_LUA;

	struct Test s = {
		.i = 1,
	};

	struct mScriptValue a;
	struct mScriptValue b;
	struct mScriptValue* val;

	LOAD_PROGRAM("b = a.i");

	a = mSCRIPT_MAKE_S(Test, &s);
	assert_true(lua->setGlobal(lua, "a", &a));

	s.i = 1;
	assert_true(lua->run(lua));
	b = mSCRIPT_MAKE_S32(1);
	val = lua->getGlobal(lua, "b");
	assert_non_null(val);
	assert_true(b.type->equal(&b, val));
	mScriptValueDeref(val);

	s.i = 2;
	assert_true(lua->run(lua));
	b = mSCRIPT_MAKE_S32(2);
	val = lua->getGlobal(lua, "b");
	assert_non_null(val);
	assert_true(b.type->equal(&b, val));
	mScriptValueDeref(val);

	a = mSCRIPT_MAKE_CS(Test, &s);
	assert_true(lua->setGlobal(lua, "a", &a));

	s.i = 1;
	assert_true(lua->run(lua));
	b = mSCRIPT_MAKE_S32(1);
	val = lua->getGlobal(lua, "b");
	assert_non_null(val);
	assert_true(b.type->equal(&b, val));
	mScriptValueDeref(val);

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(globalStructFieldSet) {
	SETUP_LUA;

	struct Test s = {
		.i = 1,
	};

	struct mScriptValue a;
	struct mScriptValue b;

	LOAD_PROGRAM("a.i = b");

	a = mSCRIPT_MAKE_S(Test, &s);
	assert_true(lua->setGlobal(lua, "a", &a));
	s.i = 1;
	b = mSCRIPT_MAKE_S32(2);
	assert_true(lua->setGlobal(lua, "b", &b));
	assert_true(lua->run(lua));
	assert_int_equal(s.i, 2);

	a = mSCRIPT_MAKE_CS(Test, &s);
	assert_true(lua->setGlobal(lua, "a", &a));
	s.i = 1;
	b = mSCRIPT_MAKE_S32(2);
	assert_true(lua->setGlobal(lua, "b", &b));
	assert_false(lua->run(lua));
	assert_int_equal(s.i, 1);

	mScriptContextDeinit(&context);
}


M_TEST_DEFINE(globalStructMethods) {
	SETUP_LUA;

	struct Test s = {
		.i = 1,
		.ifn0 = testI0,
		.ifn1 = testI1,
		.icfn0 = testIC0,
		.vfn0 = testV0,
		.vfn1 = testV1,
	};

	struct mScriptValue a;
	struct mScriptValue b;
	struct mScriptValue* val;

	// ifn0
	LOAD_PROGRAM("b = a:ifn0()");

	a = mSCRIPT_MAKE_S(Test, &s);
	assert_true(lua->setGlobal(lua, "a", &a));
	s.i = 1;
	assert_true(lua->run(lua));
	b = mSCRIPT_MAKE_S32(1);
	val = lua->getGlobal(lua, "b");
	assert_non_null(val);
	assert_true(b.type->equal(&b, val));
	mScriptValueDeref(val);

	a = mSCRIPT_MAKE_CS(Test, &s);
	assert_true(lua->setGlobal(lua, "a", &a));
	assert_false(lua->run(lua));

	// ifn1
	LOAD_PROGRAM("b = a:ifn1(c)");

	a = mSCRIPT_MAKE_S(Test, &s);
	assert_true(lua->setGlobal(lua, "a", &a));
	s.i = 1;
	b = mSCRIPT_MAKE_S32(1);
	assert_true(lua->setGlobal(lua, "c", &b));
	assert_true(lua->run(lua));
	b = mSCRIPT_MAKE_S32(2);
	val = lua->getGlobal(lua, "b");
	assert_non_null(val);
	assert_true(b.type->equal(&b, val));
	mScriptValueDeref(val);

	b = mSCRIPT_MAKE_S32(2);
	assert_true(lua->setGlobal(lua, "c", &b));
	assert_true(lua->run(lua));
	b = mSCRIPT_MAKE_S32(3);
	val = lua->getGlobal(lua, "b");
	assert_non_null(val);
	assert_true(b.type->equal(&b, val));
	mScriptValueDeref(val);

	a = mSCRIPT_MAKE_CS(Test, &s);
	assert_true(lua->setGlobal(lua, "a", &a));
	assert_false(lua->run(lua));

	// vfn0
	LOAD_PROGRAM("a:vfn0()");

	a = mSCRIPT_MAKE_S(Test, &s);
	assert_true(lua->setGlobal(lua, "a", &a));
	s.i = 1;
	assert_true(lua->run(lua));
	assert_int_equal(s.i, 2);
	assert_true(lua->run(lua));
	assert_int_equal(s.i, 3);

	a = mSCRIPT_MAKE_CS(Test, &s);
	assert_true(lua->setGlobal(lua, "a", &a));
	assert_false(lua->run(lua));

	// vfn1
	LOAD_PROGRAM("a:vfn1(c)");

	a = mSCRIPT_MAKE_S(Test, &s);
	assert_true(lua->setGlobal(lua, "a", &a));
	s.i = 1;
	b = mSCRIPT_MAKE_S32(1);
	assert_true(lua->setGlobal(lua, "c", &b));
	assert_true(lua->run(lua));
	assert_int_equal(s.i, 2);
	b = mSCRIPT_MAKE_S32(2);
	assert_true(lua->setGlobal(lua, "c", &b));
	s.i = 1;
	assert_true(lua->run(lua));
	assert_int_equal(s.i, 3);

	a = mSCRIPT_MAKE_CS(Test, &s);
	assert_true(lua->setGlobal(lua, "a", &a));
	s.i = 1;
	b = mSCRIPT_MAKE_S32(1);
	assert_true(lua->setGlobal(lua, "c", &b));
	assert_false(lua->run(lua));
	assert_int_equal(s.i, 1);

	// icfn0
	LOAD_PROGRAM("b = a:icfn0()");

	a = mSCRIPT_MAKE_S(Test, &s);
	assert_true(lua->setGlobal(lua, "a", &a));
	s.i = 1;
	assert_true(lua->run(lua));
	b = mSCRIPT_MAKE_S32(1);
	val = lua->getGlobal(lua, "b");
	assert_non_null(val);
	assert_true(b.type->equal(&b, val));
	mScriptValueDeref(val);

	a = mSCRIPT_MAKE_CS(Test, &s);
	assert_true(lua->setGlobal(lua, "a", &a));
	s.i = 1;
	assert_true(lua->run(lua));
	b = mSCRIPT_MAKE_S32(1);
	val = lua->getGlobal(lua, "b");
	assert_non_null(val);
	assert_true(b.type->equal(&b, val));
	mScriptValueDeref(val);

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(errorReporting) {
	SETUP_LUA;

	assert_null(lua->getError(lua));

	LOAD_PROGRAM("assert(false)");

	assert_false(lua->run(lua));
	const char* errorBuffer = lua->getError(lua);
	assert_non_null(errorBuffer);
	assert_non_null(strstr(errorBuffer, "assertion failed"));

	LOAD_PROGRAM("assert(true)");

	assert_true(lua->run(lua));
	assert_null(lua->getError(lua));

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(tableLookup) {
	SETUP_LUA;

	assert_null(lua->getError(lua));
	struct mScriptValue* table = mScriptValueAlloc(mSCRIPT_TYPE_MS_TABLE);
	assert_non_null(table);
	struct mScriptValue* val;

	mScriptContextSetGlobal(&context, "t", table);

	val = mScriptValueAlloc(mSCRIPT_TYPE_MS_S64);
	val->value.s64 = 0;
	assert_true(mScriptTableInsert(table, &mSCRIPT_MAKE_S64(0), val));
	mScriptValueDeref(val);

	val = mScriptStringCreateFromASCII("t");
	assert_true(mScriptTableInsert(table, &mSCRIPT_MAKE_CHARP("t"), val));
	mScriptValueDeref(val);

	val = mScriptValueAlloc(mSCRIPT_TYPE_MS_TABLE);
	assert_true(mScriptTableInsert(table, &mSCRIPT_MAKE_CHARP("sub"), val));
	mScriptValueDeref(val);

	table = val;
	val = mScriptStringCreateFromASCII("t");
	assert_true(mScriptTableInsert(table, &mSCRIPT_MAKE_CHARP("t"), val));
	mScriptValueDeref(val);

	TEST_PROGRAM("assert(t)");
	TEST_PROGRAM("assert(t['t'] ~= nil)");
	TEST_PROGRAM("assert(t['t'] == 't')");
	TEST_PROGRAM("assert(t.t == 't')");
	TEST_PROGRAM("assert(t['x'] == nil)");
	TEST_PROGRAM("assert(t.x == nil)");
	TEST_PROGRAM("assert(t.sub ~= nil)");
	TEST_PROGRAM("assert(t.sub.t ~= nil)");
	TEST_PROGRAM("assert(t.sub.t == 't')");
	TEST_PROGRAM("assert(t[0] ~= nil)");
	TEST_PROGRAM("assert(t[0] == 0)");
	TEST_PROGRAM("assert(t[1] == nil)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(tableIterate) {
	SETUP_LUA;

	assert_null(lua->getError(lua));
	struct mScriptValue* table = mScriptValueAlloc(mSCRIPT_TYPE_MS_TABLE);
	assert_non_null(table);
	struct mScriptValue* val;
	struct mScriptValue* key;

	mScriptContextSetGlobal(&context, "t", table);

	int i;
	for (i = 0; i < 50; ++i) {
		val = mScriptValueAlloc(mSCRIPT_TYPE_MS_S64);
		val->value.s64 = 1LL << i;
		key = mScriptValueAlloc(mSCRIPT_TYPE_MS_S32);
		key->value.s32 = i;
		assert_true(mScriptTableInsert(table, key, val));
		mScriptValueDeref(key);
		mScriptValueDeref(val);
	}
	assert_int_equal(mScriptTableSize(table), 50);

	TEST_PROGRAM("assert(t)");
	TEST_PROGRAM("assert(#t == 50)");
	TEST_PROGRAM(
		"i = 0\n"
		"z = 0\n"
		"for k, v in pairs(t) do\n"
		"	i = i + 1\n"
		"	z = z + v\n"
		"	assert((1 << k) == v)\n"
		"end\n"
	);

	TEST_PROGRAM("assert(i == #t)");
	TEST_PROGRAM("assert(z == (1 << #t) - 1)");

	mScriptContextDeinit(&context);
}

M_TEST_DEFINE(callList) {
	SETUP_LUA;

	struct mScriptValue a = mSCRIPT_MAKE_S32(6);
	struct mScriptValue* val;

	assert_true(lua->setGlobal(lua, "sum", &boundSum));
	TEST_PROGRAM("a = sum({1, 2, 3})");
	assert_null(lua->getError(lua));

	val = lua->getGlobal(lua, "a");
	assert_non_null(val);
	assert_true(mSCRIPT_TYPE_MS_S32->equal(&a, val));
	mScriptValueDeref(val);

	mScriptContextDeinit(&context);
}

M_TEST_SUITE_DEFINE_SETUP_TEARDOWN(mScriptLua,
	cmocka_unit_test(create),
	cmocka_unit_test(loadGood),
	cmocka_unit_test(loadBadSyntax),
	cmocka_unit_test(runNop),
	cmocka_unit_test(getGlobal),
	cmocka_unit_test(setGlobal),
	cmocka_unit_test(callLuaFunc),
	cmocka_unit_test(callCFunc),
	cmocka_unit_test(globalStructFieldGet),
	cmocka_unit_test(globalStructFieldSet),
	cmocka_unit_test(globalStructMethods),
	cmocka_unit_test(errorReporting),
	cmocka_unit_test(tableLookup),
	cmocka_unit_test(tableIterate),
	cmocka_unit_test(callList),
)
