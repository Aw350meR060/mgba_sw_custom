/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/core.h>
#include <mgba/core/scripting.h>
#include <mgba/core/version.h>
#include <mgba/script/context.h>

struct mScriptContext context;
struct Table types;
FILE* out;

void explainValue(struct mScriptValue* value, const char* name, int level);
void explainType(struct mScriptType* type, int level);

void addTypesFromTuple(const struct mScriptTypeTuple*);
void addTypesFromTable(struct Table*);

void addType(const struct mScriptType* type) {
	if (HashTableLookup(&types, type->name) || type->isConst) {
		return;
	}
	HashTableInsert(&types, type->name, (struct mScriptType*) type);
	switch (type->base) {
	case mSCRIPT_TYPE_FUNCTION:
		addTypesFromTuple(&type->details.function.parameters);
		addTypesFromTuple(&type->details.function.returnType);
		break;
	case mSCRIPT_TYPE_OBJECT:
		mScriptClassInit(type->details.cls);
		if (type->details.cls->parent) {
			addType(type->details.cls->parent);
		}
		addTypesFromTable(&type->details.cls->instanceMembers);
		break;
	case mSCRIPT_TYPE_OPAQUE:
	case mSCRIPT_TYPE_WRAPPER:
		if (type->details.type) {
			addType(type->details.type);
		}
	case mSCRIPT_TYPE_VOID:
	case mSCRIPT_TYPE_SINT:
	case mSCRIPT_TYPE_UINT:
	case mSCRIPT_TYPE_FLOAT:
	case mSCRIPT_TYPE_STRING:
	case mSCRIPT_TYPE_LIST:
	case mSCRIPT_TYPE_TABLE:
	case mSCRIPT_TYPE_WEAKREF:
		// No subtypes
		break;
	}
}

void addTypesFromTuple(const struct mScriptTypeTuple* tuple) {
	size_t i;
	for (i = 0; i < tuple->count; ++i) {
		addType(tuple->entries[i]);
	}
}

void addTypesFromTable(struct Table* table) {
	struct TableIterator iter;
	if (!HashTableIteratorStart(table, &iter)) {
		return;
	}
	do {
		struct mScriptClassMember* member = HashTableIteratorGetValue(table, &iter);
		addType(member->type);
	} while(HashTableIteratorNext(table, &iter));
}

void printchomp(const char* string, int level) {
	char indent[(level + 1) * 2 + 1];
	memset(indent, ' ', sizeof(indent) - 1);
	indent[sizeof(indent) - 1] = '\0';

	const char* start = string;
	char lineBuffer[1024];
	while (true) {
		const char* end = strchr(start, '\n');
		if (end) {
			size_t size = end - start;
			if (sizeof(lineBuffer) - 1 < size) {
				size = sizeof(lineBuffer) - 1;
			}
			strncpy(lineBuffer, start, size);
			lineBuffer[size] = '\0';
			fprintf(out, "%s%s\n", indent, lineBuffer);
		} else {
			fprintf(out, "%s%s\n", indent, start);
			break;
		}
		start = end + 1;
		if (!*end) {
			break;
		}
	}
}

bool printval(const struct mScriptValue* value, char* buffer, size_t bufferSize) {
	struct mScriptValue sval;
	switch (value->type->base) {
	case mSCRIPT_TYPE_SINT:
		if (value->type->size <= 4) {
			snprintf(buffer, bufferSize, "%"PRId32, value->value.s32);
			return true;
		}
		if (value->type->size == 8) {
			snprintf(buffer, bufferSize, "%"PRId64, value->value.s64);
			return true;
		}
		return false;
	case mSCRIPT_TYPE_UINT:
		if (value->type->size <= 4) {
			snprintf(buffer, bufferSize, "%"PRIu32, value->value.u32);
			return true;
		}
		if (value->type->size == 8) {
			snprintf(buffer, bufferSize, "%"PRIu64, value->value.u64);
			return true;
		}
		return false;
	case mSCRIPT_TYPE_FLOAT:
		if (value->type->size <= 4) {
			snprintf(buffer, bufferSize, "%g", value->value.f32);
			return true;
		}
		if (value->type->size == 8) {
			snprintf(buffer, bufferSize, "%g", value->value.f64);
			return true;
		}
		return false;
	case mSCRIPT_TYPE_STRING:
		if (!mScriptCast(mSCRIPT_TYPE_MS_CHARP, value, &sval)) {
			return false;
		}
		if (sval.value.copaque) {
			snprintf(buffer, bufferSize, "\"%s\"", (const char*) sval.value.copaque);
		} else {
			snprintf(buffer, bufferSize, "null");
		}
		return true;
	case mSCRIPT_TYPE_VOID:
		snprintf(buffer, bufferSize, "null");
		return true;
	case mSCRIPT_TYPE_OPAQUE:
	case mSCRIPT_TYPE_LIST:
	case mSCRIPT_TYPE_TABLE:
		// Not scalar or string values
		return false;
	}
	return false;
}

void explainTable(struct mScriptValue* value, const char* name, int level) {
	char indent[(level + 1) * 2 + 1];
	memset(indent, ' ', sizeof(indent) - 1);
	indent[sizeof(indent) - 1] = '\0';

	struct TableIterator iter;
	if (mScriptTableIteratorStart(value, &iter)) {
		do {
			char keyval[1024];
			struct mScriptValue* k = mScriptTableIteratorGetKey(value, &iter);
			printval(k, keyval, sizeof(keyval));
			fprintf(out, "%s- key: %s\n", indent, keyval);
			struct mScriptValue* v = mScriptTableIteratorGetValue(value, &iter);

			struct mScriptValue string;
			if (mScriptCast(mSCRIPT_TYPE_MS_CHARP, k, &string)) {
				snprintf(keyval, sizeof(keyval), "%s.%s", name, (const char*) string.value.opaque);
				explainValue(v, keyval, level + 1);
			} else {
				explainValue(v, NULL, level + 1);
			}
		} while (mScriptTableIteratorNext(value, &iter));
	}
}

void explainClass(struct mScriptTypeClass* cls, int level) {
	char indent[(level + 1) * 2 + 1];
	memset(indent, ' ', sizeof(indent) - 1);
	indent[sizeof(indent) - 1] = '\0';

	if (cls->parent) {
		fprintf(out, "%sparent: %s\n", indent, cls->parent->name);
	}
	if (cls->docstring) {
		if (strchr(cls->docstring, '\n')) {
			fprintf(out, "%scomment: |-\n", indent);
			printchomp(cls->docstring, level + 1);
		} else {
			fprintf(out, "%scomment: \"%s\"\n", indent, cls->docstring);
		}
	}

	fprintf(out, "%smembers:\n", indent);
	const char* docstring = NULL;
	const struct mScriptClassInitDetails* details;
	size_t i;
	for (i = 0; cls->details[i].type != mSCRIPT_CLASS_INIT_END; ++i) {
		details = &cls->details[i];
		switch (details->type) {
		case mSCRIPT_CLASS_INIT_DOCSTRING:
			docstring = details->info.comment;
			break;
		case mSCRIPT_CLASS_INIT_INSTANCE_MEMBER:
			fprintf(out, "%s  %s:\n", indent, details->info.member.name);
			if (docstring) {
				fprintf(out, "%s    comment: \"%s\"\n", indent, docstring);
				docstring = NULL;
			}
			fprintf(out, "%s    type: %s\n", indent, details->info.member.type->name);
			break;
		case mSCRIPT_CLASS_INIT_END:
			break;
		}
	}
}

void explainObject(struct mScriptValue* value, int level) {
	char indent[(level + 2) * 2 + 1];
	memset(indent, ' ', sizeof(indent) - 1);
	indent[sizeof(indent) - 1] = '\0';

	struct mScriptTypeClass* cls = value->type->details.cls;
	const struct mScriptClassInitDetails* details;
	size_t i;
	for (i = 0; cls->details[i].type != mSCRIPT_CLASS_INIT_END; ++i) {
		struct mScriptValue member;
		details = &cls->details[i];

		if (cls->details[i].type != mSCRIPT_CLASS_INIT_INSTANCE_MEMBER) {
			continue;
		}
		fprintf(out, "%s%s:\n", indent, details->info.member.name);
		addType(details->info.member.type);
		if (mScriptObjectGet(value, details->info.member.name, &member)) {
			struct mScriptValue* unwrappedMember;
			if (member.type->base == mSCRIPT_TYPE_WRAPPER) {
				unwrappedMember = mScriptValueUnwrap(&member);
				explainValue(unwrappedMember, NULL, level + 2);
			} else {
				explainValue(&member, NULL, level + 2);
			}
		}
	}
}

void explainValue(struct mScriptValue* value, const char* name, int level) {
	char valstring[1024];
	char indent[(level + 1) * 2 + 1];
	memset(indent, ' ', sizeof(indent) - 1);
	indent[sizeof(indent) - 1] = '\0';
	value = mScriptContextAccessWeakref(&context, value);
	addType(value->type);
	fprintf(out, "%stype: %s\n", indent, value->type->name);

	const char* docstring = NULL;
	if (name) {
		docstring = mScriptContextGetDocstring(&context, name);
	}
	if (docstring) {
		fprintf(out, "%scomment: \"%s\"\n", indent, docstring);
	}

	switch (value->type->base) {
	case mSCRIPT_TYPE_TABLE:
		fprintf(out, "%svalue:\n", indent);
		explainTable(value, name, level);
		break;
	case mSCRIPT_TYPE_SINT:
	case mSCRIPT_TYPE_UINT:
	case mSCRIPT_TYPE_STRING:
		printval(value, valstring, sizeof(valstring));
		fprintf(out, "%svalue: %s\n", indent, valstring);
		break;
	case mSCRIPT_TYPE_OBJECT:
		fprintf(out, "%svalue:\n", indent);
		explainObject(value, level);
		break;
	default:
		break;
	}
}

void explainTypeTuple(struct mScriptTypeTuple* tuple, int level) {
	char indent[(level + 1) * 2 + 1];
	memset(indent, ' ', sizeof(indent) - 1);
	indent[sizeof(indent) - 1] = '\0';
	fprintf(out, "%svariable: %s\n", indent, tuple->variable ? "yes" : "no");
	fprintf(out, "%slist:\n", indent);
	size_t i;
	for (i = 0; i < tuple->count; ++i) {
		if (tuple->names[i]) {
			fprintf(out, "%s- name: %s\n", indent, tuple->names[i]);
			fprintf(out, "%s  type: %s\n", indent, tuple->entries[i]->name);
		} else {
			fprintf(out, "%s- type: %s\n", indent, tuple->entries[i]->name);
		}
		if (tuple->defaults && tuple->defaults[i].type) {
			char defaultValue[128];
			printval(&tuple->defaults[i], defaultValue, sizeof(defaultValue));
			fprintf(out, "%s  default: %s\n", indent, defaultValue);
		}
	}
}

void explainType(struct mScriptType* type, int level) {
	char indent[(level + 1) * 2 + 1];
	memset(indent, ' ', sizeof(indent) - 1);
	indent[sizeof(indent) - 1] = '\0';
	fprintf(out, "%sbase: ", indent);
	switch (type->base) {
	case mSCRIPT_TYPE_SINT:
		fputs("sint\n", out);
		break;
	case mSCRIPT_TYPE_UINT:
		fputs("uint\n", out);
		break;
	case mSCRIPT_TYPE_FLOAT:
		fputs("float\n", out);
		break;
	case mSCRIPT_TYPE_STRING:
		fputs("string\n", out);
		break;
	case mSCRIPT_TYPE_FUNCTION:
		fputs("function\n", out);
		fprintf(out, "%sparameters:\n", indent);
		explainTypeTuple(&type->details.function.parameters, level + 1);
		fprintf(out, "%sreturn:\n", indent);
		explainTypeTuple(&type->details.function.returnType, level + 1);
		break;
	case mSCRIPT_TYPE_OPAQUE:
		fputs("opaque\n", out);
		break;
	case mSCRIPT_TYPE_OBJECT:
		fputs("object\n", out);
		explainClass(type->details.cls, level);
		break;
	case mSCRIPT_TYPE_LIST:
		fputs("list\n", out);
		break;
	case mSCRIPT_TYPE_TABLE:
		fputs("table\n", out);
		break;
	case mSCRIPT_TYPE_WRAPPER:
		fputs("wrapper\n", out);
		break;
	case mSCRIPT_TYPE_WEAKREF:
		fputs("weakref\n", out);
		break;
	case mSCRIPT_TYPE_VOID:
		fputs("void\n", out);
		break;
	}
}

bool call(struct mScriptValue* obj, const char* method, struct mScriptFrame* frame) {
	struct mScriptValue fn;
	if (!mScriptObjectGet(obj, method, &fn)) {
		return false;
	}
	struct mScriptValue* this = mScriptListAppend(&frame->arguments);
	this->type = mSCRIPT_TYPE_MS_WRAPPER;
	this->refs = mSCRIPT_VALUE_UNREF;
	this->flags = 0;
	this->value.opaque = obj;
	return mScriptInvoke(&fn, frame);
}

void explainCore(struct mCore* core) {
	struct mScriptValue wrapper;
	size_t size;
	size_t i;
	mScriptContextAttachCore(&context, core);

	struct mScriptValue* emu = mScriptContextGetGlobal(&context, "emu");
	addType(emu->type);

	if (mScriptObjectGet(emu, "memory", &wrapper)) {
		struct mScriptValue* memory = mScriptValueUnwrap(&wrapper);
		struct TableIterator iter;
		fputs("    memory:\n", out);
		if (mScriptTableIteratorStart(memory, &iter)) {
			do {
				struct mScriptValue* name = mScriptTableIteratorGetKey(memory, &iter);
				struct mScriptValue* value = mScriptTableIteratorGetValue(memory, &iter);

				fprintf(out, "      %s:\n", name->value.string->buffer);
				value = mScriptContextAccessWeakref(&context, value);

				struct mScriptFrame frame;
				uint32_t baseVal;
				struct mScriptValue* shortName;

				mScriptFrameInit(&frame);
				call(value, "base", &frame);
				mScriptPopU32(&frame.returnValues, &baseVal);
				mScriptFrameDeinit(&frame);

				mScriptFrameInit(&frame);
				call(value, "name", &frame);
				shortName = mScriptValueUnwrap(mScriptListGetPointer(&frame.returnValues, 0));
				mScriptFrameDeinit(&frame);

				fprintf(out, "        base: 0x%x\n", baseVal);
				fprintf(out, "        name: \"%s\"\n", shortName->value.string->buffer);

				mScriptValueDeref(shortName);
			} while (mScriptTableIteratorNext(memory, &iter));
		}
	}

	const struct mCoreRegisterInfo* registers;
	size = core->listRegisters(core, &registers);
	if (size) {
		fputs("    registers:\n", out);
		for (i = 0; i < size; ++i) {
			if (strncmp(registers[i].name, "spsr", 4) == 0) {
				// SPSR access is not implemented yet
				continue;
			}
			fprintf(out, "    - name: \"%s\"\n", registers[i].name);
			if (registers[i].aliases && registers[i].aliases[0]) {
				size_t alias;
				fputs("      aliases:\n", out);
				for (alias = 0; registers[i].aliases[alias]; ++alias) {
					fprintf(out, "      - \"%s\"\n", registers[i].aliases[alias]);
				}
			}
			fprintf(out, "      width: %u\n", registers[i].width);
		}
	}
	mScriptContextDetachCore(&context);
}

int main(int argc, char* argv[]) {
	out = stdout;
	if (argc > 1) {
		out = fopen(argv[1], "w");
		if (!out) {
			perror("Couldn't open output");
			return 1;
		}
	}

	mScriptContextInit(&context);
	mScriptContextAttachStdlib(&context);
	mScriptContextSetTextBufferFactory(&context, NULL, NULL);
	HashTableInit(&types, 0, NULL);

	addType(mSCRIPT_TYPE_MS_S8);
	addType(mSCRIPT_TYPE_MS_U8);
	addType(mSCRIPT_TYPE_MS_S16);
	addType(mSCRIPT_TYPE_MS_U16);
	addType(mSCRIPT_TYPE_MS_S32);
	addType(mSCRIPT_TYPE_MS_U32);
	addType(mSCRIPT_TYPE_MS_F32);
	addType(mSCRIPT_TYPE_MS_S64);
	addType(mSCRIPT_TYPE_MS_U64);
	addType(mSCRIPT_TYPE_MS_F64);
	addType(mSCRIPT_TYPE_MS_STR);
	addType(mSCRIPT_TYPE_MS_CHARP);
	addType(mSCRIPT_TYPE_MS_LIST);
	addType(mSCRIPT_TYPE_MS_TABLE);
	addType(mSCRIPT_TYPE_MS_WRAPPER);

	fputs("version:\n", out);
	fprintf(out, "  string: \"%s\"\n", projectVersion);
	fprintf(out, "  commit: \"%s\"\n", gitCommit);
	fputs("root:\n", out);
	struct TableIterator iter;
	if (HashTableIteratorStart(&context.rootScope, &iter)) {
		do {
			const char* name = HashTableIteratorGetKey(&context.rootScope, &iter);
			fprintf(out, "  %s:\n", name);
			struct mScriptValue* value = HashTableIteratorGetValue(&context.rootScope, &iter);
			explainValue(value, name, 1);
		} while (HashTableIteratorNext(&context.rootScope, &iter));
	}
	fputs("emu:\n", out);
	struct mCore* core;
	core = mCoreCreate(mPLATFORM_GBA);
	if (core) {
		fputs("  gba:\n", out);
		core->init(core);
		explainCore(core);
		core->deinit(core);
	}
	core = mCoreCreate(mPLATFORM_GB);
	if (core) {
		fputs("  gb:\n", out);
		core->init(core);
		explainCore(core);
		core->deinit(core);
	}
	fputs("types:\n", out);
	if (HashTableIteratorStart(&types, &iter)) {
		do {
			const char* name = HashTableIteratorGetKey(&types, &iter);
			fprintf(out, "  %s:\n", name);
			struct mScriptType* type = HashTableIteratorGetValue(&types, &iter);
			explainType(type, 1);
		} while (HashTableIteratorNext(&types, &iter));
	}

	HashTableDeinit(&types);
	mScriptContextDeinit(&context);
	return 0;
}
