/*
 * Copyright 2016, 2017 Tobias Grosser. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY TOBIAS GROSSER ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SVEN VERDOOLAEGE OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as
 * representing official policies, either expressed or implied, of
 * Tobias Grosser.
 */

#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "cpp.h"
#include "isl_config.h"

bool extensions = true;

/* Print string formatted according to "fmt" to ostream "os".
 *
 * This osprintf method allows us to use printf style formatting constructs when
 * writing to an ostream.
 */
static void osprintf(ostream &os, const char *format, ...)
{
	va_list arguments;
	char *string_pointer;
	size_t size;

	va_start(arguments, format);
	size = vsnprintf(NULL, 0, format, arguments);
	string_pointer = new char[size + 1];
	va_end(arguments);
	va_start(arguments, format);
	vsnprintf(string_pointer, size + 1, format, arguments);
	va_end(arguments);
	os << string_pointer;
	delete[] string_pointer;
}

/* Convert "l" to a string.
 */
static std::string to_string(long l)
{
	std::ostringstream strm;
	strm << l;
	return strm.str();
}

/* Generate a cpp interface based on the extracted types and functions.
 *
 * Print first a set of forward declarations for all isl wrapper
 * classes, then the declarations of the classes, and at the end all
 * implementations.
 *
 * If C++ bindings without exceptions are being generated,
 * then wrap them in an inline namespace to avoid conflicts
 * with the default C++ bindings (with exceptions).
 */
void cpp_generator::generate()
{
	ostream &os = cout;

	osprintf(os, "\n");
	osprintf(os, "namespace isl {\n\n");
	if (noexceptions)
		osprintf(os, "inline namespace noexceptions {\n\n");

	print_forward_declarations(os);
	osprintf(os, "\n");
	print_declarations(os);
	osprintf(os, "\n");
	print_implementations(os);

	if (noexceptions)
		osprintf(os, "} // namespace noexceptions\n");
	osprintf(os, "} // namespace isl\n");
}

/* Print forward declarations for all classes to "os".
*/
void cpp_generator::print_forward_declarations(ostream &os)
{
	map<string, isl_class>::iterator ci;

	osprintf(os, "// forward declarations\n");

	for (ci = classes.begin(); ci != classes.end(); ++ci)
		print_class_forward_decl(os, ci->second);
}

/* Print all declarations to "os".
 */
void cpp_generator::print_declarations(ostream &os)
{
	map<string, isl_class>::iterator ci;
	bool first = true;

	for (ci = classes.begin(); ci != classes.end(); ++ci) {
		if (first)
			first = false;
		else
			osprintf(os, "\n");

		print_class(os, ci->second);
	}
}

/* Print all implementations to "os".
 */
void cpp_generator::print_implementations(ostream &os)
{
	map<string, isl_class>::iterator ci;
	bool first = true;

	for (ci = classes.begin(); ci != classes.end(); ++ci) {
		if (first)
			first = false;
		else
			osprintf(os, "\n");

		print_class_impl(os, ci->second);
	}
}

/* If "clazz" is a subclass that is based on a type function,
 * then introduce a "type" field that holds the value of the type
 * corresponding to the subclass and make the fields of the class
 * accessible to the "isa" and "as" methods of the superclass.
 * In particular, "isa" needs access to the type field itself,
 * while "as" needs access to the private constructor.
 */
void cpp_generator::print_subclass_type(ostream &os, const isl_class &clazz)
{
	std::string cppstring = type2cpp(clazz);
	std::string super = type2cpp(clazz.name);
	const char *cppname = cppstring.c_str();
	const char *supername = super.c_str();

	if (!clazz.is_type_subclass())
		return;

	osprintf(os, "  friend %s %s::isa<%s>();\n",
		isl_bool2cpp().c_str(), supername, cppname);
	osprintf(os, "  friend %s %s::as<%s>();\n",
		cppname, supername, cppname);
	osprintf(os, "  static const auto type = %s;\n",
		clazz.subclass_name.c_str());
}

/* Print declarations for class "clazz" to "os".
 *
 * If "clazz" is a subclass based on a type function,
 * then it is made to inherit from the superclass and
 * a "type" attribute is added for use in the "as" and "isa"
 * methods of the superclass.
 *
 * Conversely, if "clazz" is a superclass with a type function,
 * then declare those "as" and "isa" methods.
 *
 * The pointer to the isl object is only added for classes that
 * are not subclasses, since subclasses refer to the same isl object.
 */
void cpp_generator::print_class(ostream &os, const isl_class &clazz)
{
	const char *name = clazz.name.c_str();
	std::string cppstring = type2cpp(clazz);
	const char *cppname = cppstring.c_str();

	osprintf(os, "// declarations for isl::%s\n", cppname);

	print_class_factory_decl(os, clazz);
	osprintf(os, "\n");
	osprintf(os, "class %s ", cppname);
	if (clazz.is_type_subclass())
		osprintf(os, ": public %s ", type2cpp(clazz.name).c_str());
	osprintf(os, "{\n");
	print_subclass_type(os, clazz);
	print_class_factory_decl(os, clazz, "  friend ");
	osprintf(os, "\n");
	osprintf(os, "protected:\n");
	if (!clazz.is_type_subclass()) {
		osprintf(os, "  %s *ptr = nullptr;\n", name);
		osprintf(os, "\n");
	}
	print_protected_constructors_decl(os, clazz);
	osprintf(os, "\n");
	osprintf(os, "public:\n");
	print_public_constructors_decl(os, clazz);
	print_constructors_decl(os, clazz);
	print_copy_assignment_decl(os, clazz);
	print_destructor_decl(os, clazz);
	print_ptr_decl(os, clazz);
	print_downcast_decl(os, clazz);
	print_get_ctx_decl(os);
	print_str_decl(os, clazz);
	osprintf(os, "\n");
	print_methods_decl(os, clazz);

	osprintf(os, "  typedef %s* isl_ptr_t;\n", name);
	osprintf(os, "};\n");
}

/* Print forward declaration of class "clazz" to "os".
 */
void cpp_generator::print_class_forward_decl(ostream &os,
	const isl_class &clazz)
{
	std::string cppstring = type2cpp(clazz);
	const char *cppname = cppstring.c_str();

	osprintf(os, "class %s;\n", cppname);
}

/* Print global factory functions to "os".
 *
 * Each class has two global factory functions:
 *
 * 	isl::set manage(__isl_take isl_set *ptr);
 * 	isl::set manage_copy(__isl_keep isl_set *ptr);
 *
 * A user can construct isl C++ objects from a raw pointer and indicate whether
 * they intend to take the ownership of the object or not through these global
 * factory functions. This ensures isl object creation is very explicit and
 * pointers are not converted by accident. Thanks to overloading, manage() and
 * manage_copy() can be called on any isl raw pointer and the corresponding
 * object is automatically created, without the user having to choose the right
 * isl object type.
 *
 * For a subclass based on a type function, no factory functions
 * are introduced because they share the C object type with
 * the superclass.
 */
void cpp_generator::print_class_factory_decl(ostream &os,
	const isl_class &clazz, const std::string &prefix)
{
	const char *name = clazz.name.c_str();
	std::string cppstring = type2cpp(clazz);
	const char *cppname = cppstring.c_str();

	if (clazz.is_type_subclass())
		return;

	os << prefix;
	osprintf(os, "inline isl::%s manage(__isl_take %s *ptr);\n", cppname,
		 name);
	os << prefix;
	osprintf(os, "inline isl::%s manage_copy(__isl_keep %s *ptr);\n",
		cppname, name);
}

/* Print declarations of protected constructors for class "clazz" to "os".
 *
 * Each class has currently one protected constructor:
 *
 * 	1) Constructor from a plain isl_* C pointer
 *
 * Example:
 *
 * 	set(__isl_take isl_set *ptr);
 *
 * The raw pointer constructor is kept protected. Object creation is only
 * possible through isl::manage() or isl::manage_copy().
 */
void cpp_generator::print_protected_constructors_decl(ostream &os,
	const isl_class &clazz)
{
	const char *name = clazz.name.c_str();
	std::string cppstring = type2cpp(clazz);
	const char *cppname = cppstring.c_str();

	osprintf(os, "  inline explicit %s(__isl_take %s *ptr);\n", cppname,
		 name);
}

/* Print declarations of public constructors for class "clazz" to "os".
 *
 * Each class currently has two public constructors:
 *
 * 	1) A default constructor
 * 	2) A copy constructor
 *
 * Example:
 *
 *	set();
 *	set(const isl::set &set);
 */
void cpp_generator::print_public_constructors_decl(ostream &os,
	const isl_class &clazz)
{
	std::string cppstring = type2cpp(clazz);
	const char *cppname = cppstring.c_str();
	osprintf(os, "  inline /* implicit */ %s();\n", cppname);

	osprintf(os, "  inline /* implicit */ %s(const isl::%s &obj);\n",
		 cppname, cppname);
}

/* Print declarations for constructors for class "class" to "os".
 *
 * For each isl function that is marked as __isl_constructor,
 * add a corresponding C++ constructor.
 *
 * Example:
 *
 * 	inline /\* implicit *\/ union_set(isl::basic_set bset);
 * 	inline /\* implicit *\/ union_set(isl::set set);
 * 	inline explicit val(isl::ctx ctx, long i);
 * 	inline explicit val(isl::ctx ctx, const std::string &str);
 */
void cpp_generator::print_constructors_decl(ostream &os,
       const isl_class &clazz)
{
	set<FunctionDecl *>::const_iterator in;
	const set<FunctionDecl *> &constructors = clazz.constructors;

	for (in = constructors.begin(); in != constructors.end(); ++in) {
		FunctionDecl *cons = *in;
		string fullname = cons->getName();
		function_kind kind = function_kind_constructor;

		print_method_decl(os, clazz, fullname, cons, kind);
	}
}

/* Print declarations of copy assignment operator for class "clazz"
 * to "os".
 *
 * Each class has one assignment operator.
 *
 * 	isl:set &set::operator=(isl::set obj)
 *
 */
void cpp_generator::print_copy_assignment_decl(ostream &os,
	const isl_class &clazz)
{
	std::string cppstring = type2cpp(clazz);
	const char *cppname = cppstring.c_str();

	osprintf(os, "  inline isl::%s &operator=(isl::%s obj);\n", cppname,
		 cppname);
}

/* Print declaration of destructor for class "clazz" to "os".
 *
 * No explicit destructor is needed for type based subclasses.
 */
void cpp_generator::print_destructor_decl(ostream &os, const isl_class &clazz)
{
	std::string cppstring = type2cpp(clazz);
	const char *cppname = cppstring.c_str();

	if (clazz.is_type_subclass())
		return;

	osprintf(os, "  inline ~%s();\n", cppname);
}

/* Print declaration of pointer functions for class "clazz" to "os".
 * Since type based subclasses share the pointer with their superclass,
 * they can also reuse these functions from the superclass.
 *
 * To obtain a raw pointer three functions are provided:
 *
 * 	1) __isl_give isl_set *copy()
 *
 * 	  Returns a pointer to a _copy_ of the internal object
 *
 * 	2) __isl_keep isl_set *get()
 *
 * 	  Returns a pointer to the internal object
 *
 * 	3) __isl_give isl_set *release()
 *
 * 	  Returns a pointer to the internal object and resets the
 * 	  internal pointer to nullptr.
 *
 * We also provide functionality to explicitly check if a pointer is
 * currently managed by this object.
 *
 * 	4) bool is_null()
 *
 * 	  Check if the current object is a null pointer.
 *
 * 	4) explicit operator bool()
 *
 * 	  Check if the current object represents a valid isl object,
 *	  i.e., if it is not a null pointer.
 *
 * The functions get() and release() model the value_ptr proposed in
 * http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3339.pdf.
 * The copy() function is an extension to allow the user to explicitly
 * copy the underlying object.
 *
 * Also generate a declaration to delete copy() for r-values, for
 * r-values release() should be used to avoid unnecessary copies.
 */
void cpp_generator::print_ptr_decl(ostream &os, const isl_class &clazz)
{
	const char *name = clazz.name.c_str();

	if (clazz.is_type_subclass())
		return;

	osprintf(os, "  inline __isl_give %s *copy() const &;\n", name);
	osprintf(os, "  inline __isl_give %s *copy() && = delete;\n", name);
	osprintf(os, "  inline __isl_keep %s *get() const;\n", name);
	osprintf(os, "  inline __isl_give %s *release();\n", name);
	osprintf(os, "  inline bool is_null() const;\n");
	osprintf(os, "  inline explicit operator bool() const;\n");
}

/* Print declarations for the "as" and "isa" methods, if "clazz"
 * is a superclass with a type function.
 *
 * "isa" checks whether an object is of a given subclass type.
 * "as" tries to cast an object to a given subclass type, returning
 * an invalid object if the object is not of the given type.
 */
void cpp_generator::print_downcast_decl(ostream &os, const isl_class &clazz)
{
	if (!clazz.fn_type)
		return;

	osprintf(os, "  template <class T> inline %s isa();\n",
		isl_bool2cpp().c_str());
	osprintf(os, "  template <class T> inline T as();\n");
}

/* Print the declaration of the get_ctx method.
 */
void cpp_generator::print_get_ctx_decl(ostream &os)
{
	osprintf(os, "  inline isl::ctx get_ctx() const;\n");
}

void cpp_generator::print_str_decl(ostream &os, const isl_class &clazz)
{
	if (!clazz.fn_to_str)
		return;

	osprintf(os, "  inline std::string to_str() const;\n");
}

/* Print declarations for methods in class "clazz" to "os".
 */
void cpp_generator::print_methods_decl(ostream &os, const isl_class &clazz)
{
	map<string, set<FunctionDecl *> >::const_iterator it;

	for (it = clazz.methods.begin(); it != clazz.methods.end(); ++it)
		print_method_group_decl(os, clazz, it->first, it->second);
}

/* Print declarations for methods "methods" of name "fullname" in class "clazz"
 * to "os".
 *
 * "fullname" is the name of the generated C++ method.  It commonly corresponds
 * to the isl name, with the object type prefix dropped.
 * In case of overloaded methods, the result type suffix has also been removed.
 */
void cpp_generator::print_method_group_decl(ostream &os, const isl_class &clazz,
	const string &fullname, const set<FunctionDecl *> &methods)
{
	set<FunctionDecl *>::const_iterator it;

	for (it = methods.begin(); it != methods.end(); ++it) {
		function_kind kind = get_method_kind(clazz, *it);
		print_method_decl(os, clazz, fullname, *it, kind);
	}
}

/* Print declarations for "method" in class "clazz" to "os".
 *
 * "fullname" is the name of the generated C++ method.  It commonly corresponds
 * to the isl name, with the object type prefix dropped.
 * In case of overloaded methods, the result type suffix has also been removed.
 *
 * "kind" specifies the kind of method that should be generated.
 */
void cpp_generator::print_method_decl(ostream &os, const isl_class &clazz,
	const string &fullname, FunctionDecl *method, function_kind kind)
{
	print_method_header(os, clazz, method, fullname, true, kind);
}

/* Print implementations for class "clazz" to "os".
 */
void cpp_generator::print_class_impl(ostream &os, const isl_class &clazz)
{
	std::string cppstring = type2cpp(clazz);
	const char *cppname = cppstring.c_str();

	osprintf(os, "// implementations for isl::%s\n", cppname);

	print_class_factory_impl(os, clazz);
	osprintf(os, "\n");
	print_public_constructors_impl(os, clazz);
	osprintf(os, "\n");
	print_protected_constructors_impl(os, clazz);
	osprintf(os, "\n");
	print_constructors_impl(os, clazz);
	osprintf(os, "\n");
	print_copy_assignment_impl(os, clazz);
	osprintf(os, "\n");
	print_destructor_impl(os, clazz);
	osprintf(os, "\n");
	print_ptr_impl(os, clazz);
        if (extensions) {
          osprintf(os, "\n");
          print_operators_impl(os, clazz);
          osprintf(os, "\n");
          print_str_impl(os, clazz);
        }
	osprintf(os, "\n");
	if (print_downcast_impl(os, clazz))
		osprintf(os, "\n");
	print_get_ctx_impl(os, clazz);
	osprintf(os, "\n");
	print_methods_impl(os, clazz);
}

/* Print code for throwing an exception on NULL input.
 */
static void print_throw_NULL_input(ostream &os)
{
	osprintf(os,
	    "    throw isl::exception::create(isl_error_invalid,\n"
	    "        \"NULL input\", __FILE__, __LINE__);\n");
}

/* Print implementation of global factory functions to "os".
 *
 * Each class has two global factory functions:
 *
 * 	isl::set manage(__isl_take isl_set *ptr);
 * 	isl::set manage_copy(__isl_keep isl_set *ptr);
 *
 * Unless C++ bindings without exceptions are being generated,
 * both functions require the argument to be non-NULL.
 * An exception is thrown if anything went wrong during the copying
 * in manage_copy.
 *
 * For a subclass based on a type function, no factory functions
 * are introduced because they share the C object type with
 * the superclass.
 */
void cpp_generator::print_class_factory_impl(ostream &os,
	const isl_class &clazz)
{
	const char *name = clazz.name.c_str();
	std::string cppstring = type2cpp(clazz);
	const char *cppname = cppstring.c_str();

	if (clazz.is_type_subclass())
		return;

	osprintf(os, "isl::%s manage(__isl_take %s *ptr) {\n", cppname, name);
	if (!noexceptions) {
		osprintf(os, "  if (!ptr)\n");
		print_throw_NULL_input(os);
	}
	osprintf(os, "  return %s(ptr);\n", cppname);
	osprintf(os, "}\n");

	osprintf(os, "isl::%s manage_copy(__isl_keep %s *ptr) {\n", cppname,
		name);
	if (!noexceptions) {
		osprintf(os, "  if (!ptr)\n");
		print_throw_NULL_input(os);
		osprintf(os, "  auto ctx = %s_get_ctx(ptr);\n", name);
	}
	osprintf(os, "  ptr = %s_copy(ptr);\n", name);
	if (!noexceptions) {
		osprintf(os, "  if (!ptr)\n");
		osprintf(os,
			"    throw exception::create_from_last_error(ctx);\n");
	}
	osprintf(os, "  return %s(ptr);\n", cppname);
	osprintf(os, "}\n");
}

/* Print implementations of protected constructors for class "clazz" to "os".
 *
 * The pointer to the isl object is either initialized directly or
 * through the superclass.
 */
void cpp_generator::print_protected_constructors_impl(ostream &os,
	const isl_class &clazz)
{
	const char *name = clazz.name.c_str();
	std::string cppstring = type2cpp(clazz);
	std::string super = type2cpp(clazz.name);
	const char *cppname = cppstring.c_str();
	bool subclass = clazz.is_type_subclass();

	osprintf(os, "%s::%s(__isl_take %s *ptr)\n", cppname, cppname, name);
	if (subclass)
		osprintf(os, "    : %s(ptr) {}\n", super.c_str());
	else
		osprintf(os, "    : ptr(ptr) {}\n");
}

/* Print implementations of public constructors for class "clazz" to "os".
 *
 * The pointer to the isl object is either initialized directly or
 * through the superclass.
 *
 * Throw an exception from the copy constructor if anything went wrong
 * during the copying, if any copying is performed.
 * No exceptions are thrown if C++ bindings without exceptions
 * are being generated,
 */
void cpp_generator::print_public_constructors_impl(ostream &os,
	const isl_class &clazz)
{
	const char *name = clazz.name.c_str();
	std::string cppstring = type2cpp(clazz);
	std::string super = type2cpp(clazz.name);
	const char *cppname = cppstring.c_str();
	bool subclass = clazz.is_type_subclass();

	osprintf(os, "%s::%s()\n", cppname, cppname);
	if (subclass)
		osprintf(os, "    : %s() {}\n\n", super.c_str());
	else
		osprintf(os, "    : ptr(nullptr) {}\n\n");
	osprintf(os, "%s::%s(const isl::%s &obj)\n", cppname, cppname, cppname);
	if (subclass)
		osprintf(os, "    : %s(obj)\n", super.c_str());
	else
		osprintf(os, "    : ptr(obj.copy())\n");
	osprintf(os, "{\n");
	if (!noexceptions && !subclass) {
		osprintf(os, "  if (obj.ptr && !ptr)\n");
		osprintf(os,
			"    throw exception::create_from_last_error("
			"%s_get_ctx(obj.ptr));\n",
			name);
	}
	osprintf(os, "}\n");
}

/* Print implementations of constructors for class "clazz" to "os".
 */
void cpp_generator::print_constructors_impl(ostream &os,
       const isl_class &clazz)
{
	set<FunctionDecl *>::const_iterator in;
	const set<FunctionDecl *> constructors = clazz.constructors;

	for (in = constructors.begin(); in != constructors.end(); ++in) {
		FunctionDecl *cons = *in;
		string fullname = cons->getName();
		function_kind kind = function_kind_constructor;

		print_method_impl(os, clazz, fullname, cons, kind);
	}
}

/* Print implementation of copy assignment operator for class "clazz" to "os".
 */
void cpp_generator::print_copy_assignment_impl(ostream &os,
	const isl_class &clazz)
{
	const char *name = clazz.name.c_str();
	std::string cppstring = type2cpp(clazz);
	const char *cppname = cppstring.c_str();

	osprintf(os, "%s &%s::operator=(isl::%s obj) {\n", cppname,
		 cppname, cppname);
	osprintf(os, "  std::swap(this->ptr, obj.ptr);\n", name);
	osprintf(os, "  return *this;\n");
	osprintf(os, "}\n");
}

/* Print implementation of destructor for class "clazz" to "os".
 *
 * No explicit destructor is needed for type based subclasses.
 */
void cpp_generator::print_destructor_impl(ostream &os,
	const isl_class &clazz)
{
	const char *name = clazz.name.c_str();
	std::string cppstring = type2cpp(clazz);
	const char *cppname = cppstring.c_str();

	if (clazz.is_type_subclass())
		return;

	osprintf(os, "%s::~%s() {\n", cppname, cppname);
	osprintf(os, "  if (ptr)\n");
	osprintf(os, "    %s_free(ptr);\n", name);
	osprintf(os, "}\n");
}

/* Print implementation of ptr() functions for class "clazz" to "os".
 * Since type based subclasses share the pointer with their superclass,
 * they can also reuse these functions from the superclass.
 */
void cpp_generator::print_ptr_impl(ostream &os, const isl_class &clazz)
{
	const char *name = clazz.name.c_str();
	std::string cppstring = type2cpp(clazz);
	const char *cppname = cppstring.c_str();

	if (clazz.is_type_subclass())
		return;

	osprintf(os, "__isl_give %s *%s::copy() const & {\n", name, cppname);
	osprintf(os, "  return %s_copy(ptr);\n", name);
	osprintf(os, "}\n\n");
	osprintf(os, "__isl_keep %s *%s::get() const {\n", name, cppname);
	osprintf(os, "  return ptr;\n");
	osprintf(os, "}\n\n");
	osprintf(os, "__isl_give %s *%s::release() {\n", name, cppname);
	osprintf(os, "  %s *tmp = ptr;\n", name);
	osprintf(os, "  ptr = nullptr;\n");
	osprintf(os, "  return tmp;\n");
	osprintf(os, "}\n\n");
	osprintf(os, "bool %s::is_null() const {\n", cppname);
	osprintf(os, "  return ptr == nullptr;\n");
	osprintf(os, "}\n");
	osprintf(os, "%s::operator bool() const\n", cppname);
	osprintf(os, "{\n");
	osprintf(os, "  return !is_null();\n");
	osprintf(os, "}\n");
}

/* Print implementations for the "as" and "isa" methods, if "clazz"
 * is a superclass with a type function.
 *
 * "isa" checks whether an object is of a given subclass type.
 * "as" tries to cast an object to a given subclass type, returning
 * an invalid object if the object is not of the given type.
 *
 * If the input is an invalid object, then these methods raise
 * an exception.
 * If bindings without exceptions are being generated,
 * then an invalid isl::boolean or object is returned instead.
 *
 * Return true if anything was printed.
 */
bool cpp_generator::print_downcast_impl(ostream &os, const isl_class &clazz)
{
	std::string cppstring = type2cpp(clazz);
	const char *cppname = cppstring.c_str();

	if (!clazz.fn_type)
		return false;

	osprintf(os, "template <class T>\n");
	osprintf(os, "%s %s::isa()\n", isl_bool2cpp().c_str(), cppname);
	osprintf(os, "{\n");
	osprintf(os, "  if (is_null())\n");
	if (noexceptions)
		osprintf(os, "    return isl::boolean();\n");
	else
		print_throw_NULL_input(os);
	osprintf(os, "  return %s(get()) == T::type;\n",
		clazz.fn_type->getNameAsString().c_str());
	osprintf(os, "}\n");

	osprintf(os, "template <class T>\n");
	osprintf(os, "T %s::as()\n", cppname);
	osprintf(os, "{\n");
	if (noexceptions) {
		osprintf(os, "  if (is_null())\n");
		osprintf(os, "    T();\n");
	}
	osprintf(os, "  return isa<T>() ? T(copy()) : T();\n");
	osprintf(os, "}\n");

	return true;
}

/* Print the implementation of the get_ctx method.
 */
void cpp_generator::print_get_ctx_impl(ostream &os, const isl_class &clazz)
{
	const char *name = clazz.name.c_str();
	std::string cppstring = type2cpp(clazz);
	const char *cppname = cppstring.c_str();

	osprintf(os, "isl::ctx %s::get_ctx() const {\n", cppname);
	osprintf(os, "  return isl::ctx(%s_get_ctx(ptr));\n", name);
	osprintf(os, "}\n");
}

void cpp_generator::print_str_impl(ostream &os, const isl_class &clazz)
{
	if (!clazz.fn_to_str)
		return;

	const char *name = clazz.name.c_str();
	std::string cppstring = type2cpp(clazz);
	const char *cppname = cppstring.c_str();
	osprintf(os, "std::string %s::to_str() const {\n", cppname);
	osprintf(os, "  char *Tmp = %s_to_str(get());\n", name, name);
	osprintf(os, "  if (!Tmp)\n");
	osprintf(os, "    return \"\";\n");
	osprintf(os, "  std::string S(Tmp);\n");
	osprintf(os, "  free(Tmp);\n");
	osprintf(os, "  return S;\n");
	osprintf(os, "}\n");
	osprintf(os, "\n");
}

void cpp_generator::print_operators_impl(ostream &os, const isl_class &clazz)
{
	std::string cppstring = type2cpp(clazz);
	const char *cppname = cppstring.c_str();
        if (clazz.fn_to_str) {
	        osprintf(os, "inline std::ostream& operator<<(std::ostream& os, const %s& C) {\n", cppname);
        	osprintf(os, "  os << C.to_str();\n");
        	osprintf(os, "  return os;\n");
        	osprintf(os, "}\n");
        	osprintf(os, "\n");
        }
        if (clazz.fn_is_equal) {
		QualType return_type = clazz.fn_is_equal->getReturnType();
	        osprintf(os,
                         "inline %s operator==(const %s& C1, const %s& C2) {\n",
			 type2cpp(return_type).c_str(),
                         cppname,
                         cppname);
        	osprintf(os, "  return C1.is_equal(C2);\n");
        	osprintf(os, "}\n");
        	osprintf(os, "\n");
        }
}

/* Print definitions for methods of class "clazz" to "os".
 */
void cpp_generator::print_methods_impl(ostream &os, const isl_class &clazz)
{
	map<string, set<FunctionDecl *> >::const_iterator it;
	bool first = true;

	for (it = clazz.methods.begin(); it != clazz.methods.end(); ++it) {
		if (first)
			first = false;
		else
			osprintf(os, "\n");
		print_method_group_impl(os, clazz, it->first, it->second);
	}
}

/* Print definitions for methods "methods" of name "fullname" in class "clazz"
 * to "os".
 *
 * "fullname" is the name of the generated C++ method.  It commonly corresponds
 * to the isl name, with the object type prefix dropped.
 * In case of overloaded methods, the result type suffix has also been removed.
 *
 * "kind" specifies the kind of method that should be generated.
 */
void cpp_generator::print_method_group_impl(ostream &os, const isl_class &clazz,
	const string &fullname, const set<FunctionDecl *> &methods)
{
	set<FunctionDecl *>::const_iterator it;
	bool first = true;

	for (it = methods.begin(); it != methods.end(); ++it) {
		function_kind kind;
		if (first)
			first = false;
		else
			osprintf(os, "\n");
		kind = get_method_kind(clazz, *it);
		print_method_impl(os, clazz, fullname, *it, kind);
	}
}

/* Print the use of "param" to "os".
 *
 * "load_from_this_ptr" specifies whether the parameter should be loaded from
 * the this-ptr.  In case a value is loaded from a this pointer, the original
 * value must be preserved and must consequently be copied.  Values that are
 * loaded from parameters do not need to be preserved, as such values will
 * already be copies of the actual parameters.  It is consequently possible
 * to directly take the pointer from these values, which saves
 * an unnecessary copy.
 *
 * In case the parameter is a callback function, two parameters get printed,
 * a wrapper for the callback function and a pointer to the actual
 * callback function.  The wrapper is expected to be available
 * in a previously declared variable <name>_lambda, while
 * the actual callback function is expected to be stored
 * in a structure called <name>_data.
 * The caller of this function must ensure that these variables exist.
 */
void cpp_generator::print_method_param_use(ostream &os, ParmVarDecl *param,
	bool load_from_this_ptr)
{
	string name = param->getName().str();
	const char *name_str = name.c_str();
	QualType type = param->getOriginalType();

        if (extensions) {
          if (type->isEnumeralType()) {
            string typestr = type.getAsString();
            osprintf(os, "static_cast<%s>(%s)", typestr.c_str(), name_str);
            return;
          }
        }

	if (type->isIntegerType()) {
		osprintf(os, "%s", name_str);
		return;
	}

	if (is_string(type)) {
		osprintf(os, "%s.c_str()", name_str);
		return;
	}

	if (is_callback(type)) {
		osprintf(os, "%s_lambda, ", name_str);
		osprintf(os, "&%s_data", name_str);
		return;
	}

	if (!load_from_this_ptr && !is_callback(type))
		osprintf(os, "%s.", name_str);

	if (keeps(param)) {
		osprintf(os, "get()");
	} else {
		if (load_from_this_ptr)
			osprintf(os, "copy()");
		else
			osprintf(os, "release()");
	}
}

/* Print code that checks that all isl object arguments to "method" are valid
 * (not NULL) and throws an exception if they are not.
 * "kind" specifies the kind of method that is being generated.
 *
 * If bindings without exceptions are being generated,
 * then no such check is performed.
 */
void cpp_generator::print_argument_validity_check(ostream &os,
	FunctionDecl *method, function_kind kind)
{
	int n;
	bool first = true;

	if (noexceptions)
		return;

	n = method->getNumParams();
	for (int i = 0; i < n; ++i) {
		bool is_this;
		ParmVarDecl *param = method->getParamDecl(i);
		string name = param->getName().str();
		const char *name_str = name.c_str();
		QualType type = param->getOriginalType();

		is_this = i == 0 && kind == function_kind_member_method;
		if (!is_this && (is_isl_ctx(type) || !is_isl_type(type)))
			continue;

		if (first)
			osprintf(os, "  if (");
		else
			osprintf(os, " || ");

		if (is_this)
			osprintf(os, "!ptr");
		else
			osprintf(os, "%s.is_null()", name_str);

		first = false;
	}
	if (first)
		return;
	osprintf(os, ")\n");
	print_throw_NULL_input(os);
}

/* Print code for saving a copy of the isl::ctx available at the start
 * of the method "method", for use in the code printed by print_method_ctx().
 * "kind" specifies what kind of method "method" is.
 *
 * If bindings without exceptions are being generated,
 * then print_method_ctx does not get called.
 * If "method" is a member function, then "this" still has an associated
 * isl::ctx where the isl::ctx is needed, so no copy needs to be saved.
 * Similarly if the first argument of the method is an isl::ctx.
 * Otherwise, save a copy of the isl::ctx associated to the first argument
 * of isl object type.
 */
void cpp_generator::print_save_ctx(ostream &os, FunctionDecl *method,
	function_kind kind)
{
	int n;
	ParmVarDecl *param = method->getParamDecl(0);
	QualType type = param->getOriginalType();

	if (noexceptions)
		return;
	if (kind == function_kind_member_method)
		return;
	if (is_isl_ctx(type))
		return;
	n = method->getNumParams();
	for (int i = 0; i < n; ++i) {
		ParmVarDecl *param = method->getParamDecl(i);
		QualType type = param->getOriginalType();

		if (!is_isl_type(type))
			continue;
		osprintf(os, "  auto ctx = %s.get_ctx();\n",
			param->getName().str().c_str());
		return;
	}
}

/* Print code for obtaining the isl_ctx associated to method "method"
 * of class "clazz".
 * "kind" specifies what kind of method "method" is.
 *
 * If "method" is a member function, then obtain the isl_ctx from
 * the "this" object.
 * If the first argument of the method is an isl::ctx, then use that one.
 * Otherwise use the isl::ctx saved by the code generated by print_save_ctx.
 */
void cpp_generator::print_method_ctx(ostream &os, FunctionDecl *method,
	function_kind kind)
{
	ParmVarDecl *param = method->getParamDecl(0);
	QualType type = param->getOriginalType();

	if (kind == function_kind_member_method)
		osprintf(os, "get_ctx()");
	else if (is_isl_ctx(type))
		osprintf(os, "%s", param->getName().str().c_str());
	else
		osprintf(os, "ctx");
}

/* Print code to make isl not print an error message when an error occurs
 * within the current scope, since the error message will be included
 * in the exception.
 * If bindings without exceptions are being generated,
 * then leave it to the user to decide what isl should do on error.
 */
void cpp_generator::print_on_error_continue(ostream &os, FunctionDecl *method,
	function_kind kind)
{
	if (noexceptions)
		return;
	osprintf(os, "  options_scoped_set_on_error saved_on_error(");
	print_method_ctx(os, method, kind);
	osprintf(os, ", ISL_ON_ERROR_CONTINUE);\n");
}

/* Print code that checks whether the execution of the core of "method"
 * was successful.
 * "kind" specifies what kind of method "method" is.
 *
 * If bindings without exceptions are being generated,
 * then no checks are performed.
 *
 * Otherwise, first check if any of the callbacks failed with
 * an exception.  If so, the "eptr" in the corresponding data structure
 * contains the exception that was caught and that needs to be rethrown.
 * Then check if the function call failed in any other way and throw
 * the appropriate exception.
 * In particular, if the return type is isl_stat or isl_bool,
 * then a negative value indicates a failure.  If the return type
 * is an isl type, then a NULL value indicates a failure.
 */
void cpp_generator::print_exceptional_execution_check(ostream &os,
	FunctionDecl *method, function_kind kind)
{
	int n;
	bool check_null, check_neg;
	QualType return_type = method->getReturnType();

	if (noexceptions)
		return;

	n = method->getNumParams();
	for (int i = 0; i < n; ++i) {
		ParmVarDecl *param = method->getParamDecl(i);
		const char *name;

		if (!is_callback(param->getOriginalType()))
			continue;
		name = param->getName().str().c_str();
		osprintf(os, "  if (%s_data.eptr)\n", name);
		osprintf(os, "    std::rethrow_exception(%s_data.eptr);\n",
			name);
	}

	check_neg = is_isl_stat(return_type) || is_isl_bool(return_type);
	check_null = is_isl_type(return_type);
	if (!check_null && !check_neg)
		return;

	if (check_neg)
		osprintf(os, "  if (res < 0)\n");
	else
		osprintf(os, "  if (!res)\n");
	osprintf(os, "    throw exception::create_from_last_error(");
	print_method_ctx(os, method, kind);
	osprintf(os, ");\n");
}

/* If "clazz" is a subclass that is based on a type function and
 * if "type" corresponds to the superclass data type,
 * then replace "type" by the subclass data type of "clazz" and return true.
 */
bool cpp_generator::super2sub(const isl_class &clazz, string &type)
{
	if (!clazz.is_type_subclass())
		return false;

	if (type != "isl::" + type2cpp(clazz.name))
		return false;

	type = "isl::" + type2cpp(clazz);

	return true;
}

/* Print definition for "method" in class "clazz" to "os".
 *
 * "fullname" is the name of the generated C++ method.  It commonly corresponds
 * to the isl name, with the object type prefix dropped.
 * In case of overloaded methods, the result type suffix has also been removed.
 *
 * "kind" specifies the kind of method that should be generated.
 *
 * This method distinguishes three kinds of methods: member methods, static
 * methods, and constructors.
 *
 * Member methods call "method" by passing to the underlying isl function the
 * isl object belonging to "this" as first argument and the remaining arguments
 * as subsequent arguments. The result of the isl function is returned as a new
 * object if the underlying isl function returns an isl_* ptr, as a bool
 * if the isl function returns an isl_bool, as void if the isl functions
 * returns an isl_stat,
 * as std::string if the isl function returns 'const char *', and as
 * unmodified return value otherwise.
 * If C++ bindings without exceptions are being generated,
 * then an isl_bool return type is transformed into an isl::boolean and
 * an isl_stat into an isl::stat since no exceptions can be generated
 * on negative results from the isl function.
 * If "clazz" is a subclass that is based on a type function and
 * if the return type corresponds to the superclass data type,
 * then it is replaced by the subclass data type.
 *
 * Static methods call "method" by passing all arguments to the underlying isl
 * function, as no this-pointer is available. The result is a newly managed
 * isl C++ object.
 *
 * Constructors create a new object from a given set of input parameters. They
 * do not return a value, but instead update the pointer stored inside the
 * newly created object.
 *
 * If the method has a callback argument, we reduce the number of parameters
 * that are exposed by one to hide the user pointer from the interface. On
 * the C++ side no user pointer is needed, as arguments can be forwarded
 * as part of the std::function argument which specifies the callback function.
 *
 * Unless C++ bindings without exceptions are being generated,
 * the inputs of the method are first checked for being valid isl objects and
 * a copy of the associated isl::ctx is saved (if needed).
 * If any failure occurs, either during the check for the inputs or
 * during the isl function call, an exception is thrown.
 * During the function call, isl is made not to print any error message
 * because the error message is included in the exception.
 */
void cpp_generator::print_method_impl(ostream &os, const isl_class &clazz,
	const string &fullname, FunctionDecl *method, function_kind kind)
{
	string methodname = method->getName();
	int num_params = method->getNumParams();
	QualType return_type = method->getReturnType();
	string rettype_str = type2cpp(return_type);
	bool has_callback = false;
	bool returns_super = super2sub(clazz, rettype_str);

	print_method_header(os, clazz, method, fullname, false, kind);
	osprintf(os, "{\n");
	print_argument_validity_check(os, method, kind);
	print_save_ctx(os, method, kind);
	print_on_error_continue(os, method, kind);

	for (int i = 0; i < num_params; ++i) {
		ParmVarDecl *param = method->getParamDecl(i);
		if (is_callback(param->getType())) {
			has_callback = true;
			num_params -= 1;
			print_callback_local(os, param);
		}
	}

	osprintf(os, "  auto res = %s(", methodname.c_str());

	for (int i = 0; i < num_params; ++i) {
		ParmVarDecl *param = method->getParamDecl(i);
		bool load_from_this_ptr = false;

		if (i == 0 && kind == function_kind_member_method)
			load_from_this_ptr = true;

		print_method_param_use(os, param, load_from_this_ptr);

		if (i != num_params - 1)
			osprintf(os, ", ");
	}
	osprintf(os, ");\n");

	print_exceptional_execution_check(os, method, kind);
	if (kind == function_kind_constructor) {
		osprintf(os, "  ptr = res;\n");
	} else if (is_isl_type(return_type) ||
		    (noexceptions && is_isl_bool(return_type))) {
		if (returns_super)
			osprintf(os, "  return manage(res).as<%s>();\n",
				rettype_str.c_str());
		else
			osprintf(os, "  return manage(res);\n");
	} else if (has_callback) {
		osprintf(os, "  return %s(res);\n", rettype_str.c_str());
	} else if (is_string(return_type)) {
		osprintf(os, "  std::string tmp(res);\n");
		if (gives(method))
			osprintf(os, "  free(res);\n");
		osprintf(os, "  return tmp;\n");
	} else if (is_isl_enum(return_type)) {
		string typestr = return_type.getAsString();
		typestr = typestr.replace(typestr.find("isl_"), sizeof("isl_")-1, "isl::");
		osprintf(os, "  return static_cast<%s>(res);\n", typestr.c_str());

	} else {
		osprintf(os, "  return res;\n");
	}

	osprintf(os, "}\n");
}

/* Print the header for "method" in class "clazz" to "os".
 *
 * Print the header of a declaration if "is_declaration" is set, otherwise print
 * the header of a method definition.
 *
 * "fullname" is the name of the generated C++ method.  It commonly corresponds
 * to the isl name, with the object type prefix dropped.
 * In case of overloaded methods, the result type suffix has also been removed.
 *
 * "kind" specifies the kind of method that should be generated.
 *
 * This function prints headers for member methods, static methods, and
 * constructors, either for their declaration or definition.
 *
 * Member functions are declared as "const", as they do not change the current
 * object, but instead create a new object. They always retrieve the first
 * parameter of the original isl function from the this-pointer of the object,
 * such that only starting at the second parameter the parameters of the
 * original function become part of the method's interface.
 *
 * A function
 *
 * 	__isl_give isl_set *isl_set_intersect(__isl_take isl_set *s1,
 * 		__isl_take isl_set *s2);
 *
 * is translated into:
 *
 * 	inline isl::set intersect(isl::set set2) const;
 *
 * For static functions and constructors all parameters of the original isl
 * function are exposed.
 *
 * Parameters that are defined as __isl_keep or are of type string, are passed
 * as const reference, which allows the compiler to optimize the parameter
 * transfer.
 *
 * Constructors are marked as explicit using the C++ keyword 'explicit' or as
 * implicit using a comment in place of the explicit keyword. By annotating
 * implicit constructors with a comment, users of the interface are made
 * aware of the potential danger that implicit construction is possible
 * for these constructors, whereas without a comment not every user would
 * know that implicit construction is allowed in absence of an explicit keyword.
 */
void cpp_generator::print_method_header(ostream &os, const isl_class &clazz,
	FunctionDecl *method, const string &fullname, bool is_declaration,
	function_kind kind)
{
	string cname = clazz.method_name(method);
	string rettype_str = type2cpp(method->getReturnType());
	string classname = type2cpp(clazz);
	int num_params = method->getNumParams();
	int first_param = 0;

	cname = rename_method(cname);
	if (kind == function_kind_member_method)
		first_param = 1;

	if (is_declaration) {
		osprintf(os, "  ");

		if (kind == function_kind_static_method)
			osprintf(os, "static ");

		osprintf(os, "inline ");

		if (kind == function_kind_constructor) {
			if (is_implicit_conversion(clazz, method))
				osprintf(os, "/* implicit */ ");
			else
				osprintf(os, "explicit ");
		}
	}

	super2sub(clazz, rettype_str);
	if (kind != function_kind_constructor)
		osprintf(os, "%s ", rettype_str.c_str());

	if (!is_declaration)
		osprintf(os, "%s::", classname.c_str());

	if (kind != function_kind_constructor)
		osprintf(os, "%s", cname.c_str());
	else
		osprintf(os, "%s", classname.c_str());

	osprintf(os, "(");

	for (int i = first_param; i < num_params; ++i) {
		ParmVarDecl *param = method->getParamDecl(i);
		QualType type = param->getOriginalType();
		string cpptype = type2cpp(type);

		if (is_callback(type))
			num_params--;

		if (keeps(param) || is_string(type) || is_callback(type))
			osprintf(os, "const %s &%s", cpptype.c_str(),
				 param->getName().str().c_str());
		else
			osprintf(os, "%s %s", cpptype.c_str(),
				 param->getName().str().c_str());

		if (i != num_params - 1)
			osprintf(os, ", ");
	}

	osprintf(os, ")");

	if (kind == function_kind_member_method)
		osprintf(os, " const");

	if (is_declaration)
		osprintf(os, ";");
	osprintf(os, "\n");
}

/* Generate the list of argument types for a callback function of
 * type "type".  If "cpp" is set, then generate the C++ type list, otherwise
 * the C type list.
 *
 * For a callback of type
 *
 *      isl_stat (*)(__isl_take isl_map *map, void *user)
 *
 * the following C++ argument list is generated:
 *
 *      isl::map
 */
string cpp_generator::generate_callback_args(QualType type, bool cpp)
{
	std::string type_str;
	const FunctionProtoType *callback;
	int num_params;

	callback = type->getPointeeType()->getAs<FunctionProtoType>();
	num_params = callback->getNumArgs();
	if (cpp)
		num_params--;

	for (long i = 0; i < num_params; i++) {
		QualType type = callback->getArgType(i);

		if (cpp)
			type_str += type2cpp(type);
		else
			type_str += type.getAsString();

		if (!cpp)
			type_str += "arg_" + ::to_string(i);

		if (i != num_params - 1)
			type_str += ", ";
	}

	return type_str;
}

/* Generate the full cpp type of a callback function of type "type".
 *
 * For a callback of type
 *
 *      isl_stat (*)(__isl_take isl_map *map, void *user)
 *
 * the following type is generated:
 *
 *      std::function<isl::stat(isl::map)>
 */
string cpp_generator::generate_callback_type(QualType type)
{
	std::string type_str;
	const FunctionProtoType *callback = type->getPointeeType()->getAs<FunctionProtoType>();
	QualType return_type = callback->getReturnType();
	string rettype_str = type2cpp(return_type);

	type_str = "std::function<";
	type_str += rettype_str;
	type_str += "(";
	type_str += generate_callback_args(type, true);
	type_str += ")>";

	return type_str;
}

/* Print the call to the C++ callback function "call",
 * with return type "type", wrapped
 * for use inside the lambda function that is used as the C callback function,
 * in the case where C++ bindings without exceptions are being generated.
 *
 * In particular, print
 *
 *        auto ret = @call@;
 *        return isl_stat(ret);
 * or
 *        auto ret = @call@;
 *        return ret.release();
 *
 * depending on the return type.
 */
void cpp_generator::print_wrapped_call_noexceptions(ostream &os,
	const string &call, QualType rtype)
{
	osprintf(os, "    auto ret = %s;\n", call.c_str());
	if (is_isl_stat(rtype))
		osprintf(os, "    return isl_stat(ret);\n");
	else
		osprintf(os, "    return ret.release();\n");
}

/* Print the call to the C++ callback function "call",
 * with return type "type", wrapped
 * for use inside the lambda function that is used as the C callback function.
 *
 * In particular, print
 *
 *        try {
 *          @call@;
 *          return isl_stat_ok;
 *        } catch (...) {
 *          data->eptr = std::current_exception();
 *          return isl_stat_error;
 *        }
 * or
 *        try {
 *          auto ret = @call@;
 *          return ret ? isl_bool_true : isl_bool_false;;
 *        } catch (...) {
 *          data->eptr = std::current_exception();
 *          return isl_bool_error;
 *        }
 * or
 *        try {
 *          auto ret = @call@;
 *          return ret.release();
 *        } catch (...) {
 *          data->eptr = std::current_exception();
 *          return NULL;
 *        }
 *
 * depending on the return type.
 *
 * If C++ bindings without exceptions are being generated, then
 * the call is wrapped differently.
 */
void cpp_generator::print_wrapped_call(ostream &os, const string &call,
	QualType rtype)
{
	if (noexceptions)
		return print_wrapped_call_noexceptions(os, call, rtype);

	osprintf(os, "    try {\n");
	if (is_isl_stat(rtype))
		osprintf(os, "      %s;\n", call.c_str());
	else
		osprintf(os, "      auto ret = %s;\n", call.c_str());
	if (is_isl_stat(rtype))
		osprintf(os, "      return isl_stat_ok;\n");
	else if (is_isl_bool(rtype))
		osprintf(os,
			"      return ret ? isl_bool_true : isl_bool_false;\n");
	else
		osprintf(os, "      return ret.release();\n");
	osprintf(os, "    } catch (...) {\n"
		     "      data->eptr = std::current_exception();\n");
	if (is_isl_stat(rtype))
		osprintf(os, "      return isl_stat_error;\n");
	else if (is_isl_bool(rtype))
		osprintf(os, "      return isl_bool_error;\n");
	else
		osprintf(os, "      return NULL;\n");
	osprintf(os, "    }\n");
}

/* Print the local variables that are needed for a callback argument,
 * in particular, print a lambda function that wraps the callback and
 * a pointer to the actual C++ callback function.
 *
 * For a callback of the form
 *
 *      isl_stat (*fn)(__isl_take isl_map *map, void *user)
 *
 * the following lambda function is generated:
 *
 *      auto fn_lambda = [](isl_map *arg_0, void *arg_1) -> isl_stat {
 *        auto *data = static_cast<struct fn_data *>(arg_1);
 *        try {
 *          stat ret = (*data->func)(isl::manage(arg_0));
 *          return isl_stat_ok;
 *        } catch (...) {
 *          data->eptr = std::current_exception();
 *          return isl_stat_error;
 *        }
 *      };
 *
 * The pointer to the std::function C++ callback function is stored in
 * a fn_data data structure for passing to the C callback function,
 * along with an std::exception_ptr that is used to store any
 * exceptions thrown in the C++ callback.
 *
 *      struct fn_data {
 *        const std::function<stat(map)> *func;
 *        std::exception_ptr eptr;
 *      } fn_data = { &fn };
 *
 * This std::function object represents the actual user
 * callback function together with the locally captured state at the caller.
 *
 * The lambda function is expected to be used as a C callback function
 * where the lambda itself is provided as the function pointer and
 * where the user void pointer is a pointer to fn_data.
 * The std::function object is extracted from the pointer to fn_data
 * inside the lambda function.
 *
 * The std::exception_ptr object is not added to fn_data
 * if C++ bindings without exceptions are being generated.
 * The body of the generated lambda function then is as follows:
 *
 *        stat ret = (*data->func)(isl::manage(arg_0));
 *        return isl_stat(ret);
 *
 * If the C callback does not take its arguments, then
 * isl::manage_copy is used instead of isl::manage.
 */
void cpp_generator::print_callback_local(ostream &os, ParmVarDecl *param)
{
	string pname;
	QualType ptype, rtype;
	string call, c_args, cpp_args, rettype, last_idx;
	const FunctionProtoType *callback;
	int num_params;

	pname = param->getName().str();
	ptype = param->getType();

	c_args = generate_callback_args(ptype, false);
	cpp_args = generate_callback_type(ptype);

	callback = ptype->getPointeeType()->getAs<FunctionProtoType>();
	rtype = callback->getReturnType();
	rettype = rtype.getAsString();
	num_params = callback->getNumArgs();

	last_idx = ::to_string(num_params - 1);

	call = "(*data->func)(";
	for (long i = 0; i < num_params - 1; i++) {
		if (!callback_takes_arguments(callback))
			call += "isl::manage_copy";
		else
			call += "isl::manage";
		call += "(arg_" + ::to_string(i) + ")";
		if (i != num_params - 2)
			call += ", ";
	}
	call += ")";

	osprintf(os, "  struct %s_data {\n", pname.c_str());
	osprintf(os, "    const %s *func;\n", cpp_args.c_str());
	if (!noexceptions)
		osprintf(os, "    std::exception_ptr eptr;\n");
	osprintf(os, "  } %s_data = { &%s };\n", pname.c_str(), pname.c_str());
	osprintf(os, "  auto %s_lambda = [](%s) -> %s {\n",
		 pname.c_str(), c_args.c_str(), rettype.c_str());
	osprintf(os,
		 "    auto *data = static_cast<struct %s_data *>(arg_%s);\n",
		 pname.c_str(), last_idx.c_str());
	print_wrapped_call(os, call, rtype);
	osprintf(os, "  };\n");
}

/* An array listing functions that must be renamed and the function name they
 * should be renamed to. We currently rename functions in case their name would
 * match a reserved C++ keyword, which is not allowed in C++.
 */
static const char *rename_map[][2] = {
	{ "union", "unite" },
	{ "delete", "del" },
};

/* Rename method "name" in case the method name in the C++ bindings should not
 * match the name in the C bindings. We do this for example to avoid
 * C++ keywords.
 */
std::string cpp_generator::rename_method(std::string name)
{
	for (size_t i = 0; i < sizeof(rename_map) / sizeof(rename_map[0]); i++)
		if (name.compare(rename_map[i][0]) == 0)
			return rename_map[i][1];

	return name;
}

/* Translate isl class "clazz" to its corresponding C++ type.
 * Use the name of the type based subclass, if any.
 */
string cpp_generator::type2cpp(const isl_class &clazz)
{
	return type2cpp(clazz.subclass_name);
}

/* Translate type string "type_str" to its C++ name counterpart.
*/
string cpp_generator::type2cpp(string type_str)
{
	return type_str.substr(4);
}

/* Return the C++ counterpart to the isl_bool type.
 * If C++ bindings without exceptions are being generated,
 * then this is isl::boolean.  Otherwise, it is simply "bool".
 */
string cpp_generator::isl_bool2cpp()
{
	return noexceptions ? "isl::boolean" : "bool";
}

/* Translate QualType "type" to its C++ name counterpart.
 *
 * An isl_bool return type is translated into "bool",
 * while an isl_stat is translated into "void".
 * The exceptional cases are handled through exceptions.
 * If C++ bindings without exceptions are being generated, then
 * C++ counterparts of isl_bool and isl_stat need to be used instead.
 */
string cpp_generator::type2cpp(QualType type)
{
	if (is_isl_type(type))
		return "isl::" + type2cpp(type->getPointeeType().getAsString());

	if (is_isl_bool(type))
		return isl_bool2cpp();

	if (is_isl_stat(type))
		return noexceptions ? "isl::stat" : "void";

        if (extensions) {
          if (type->isEnumeralType()) {
            string typestr = type.getAsString();
            return typestr.replace(
                typestr.find("isl_"), sizeof("isl_")-1, "isl::");
          }
          else if (is_isl_ctx(type)) {
            return "isl::ctx";
          }
        }

	if (type->isIntegerType())
		return type.getAsString();

	if (is_string(type))
		return "std::string";

	if (is_callback(type))
		return generate_callback_type(type);

	die("Cannot convert type to C++ type");
}

/* Check if "subclass_type" is a subclass of "class_type".
 */
bool cpp_generator::is_subclass(QualType subclass_type,
	const isl_class &class_type)
{
	std::string type_str = subclass_type->getPointeeType().getAsString();
	std::vector<std::string> superclasses;
	std::vector<const isl_class *> parents;
	std::vector<std::string>::iterator ci;

	superclasses = generator::find_superclasses(classes[type_str].type);

	for (ci = superclasses.begin(); ci < superclasses.end(); ci++)
		parents.push_back(&classes[*ci]);

	while (!parents.empty()) {
		const isl_class *candidate = parents.back();

		parents.pop_back();

		if (&class_type == candidate)
			return true;

		superclasses = generator::find_superclasses(candidate->type);

		for (ci = superclasses.begin(); ci < superclasses.end(); ci++)
			parents.push_back(&classes[*ci]);
	}

	return false;
}

/* Check if "cons" is an implicit conversion constructor of class "clazz".
 *
 * An implicit conversion constructor is generated in case "cons" has a single
 * parameter, where the parameter type is a subclass of the class that is
 * currently being generated.
 */
bool cpp_generator::is_implicit_conversion(const isl_class &clazz,
	FunctionDecl *cons)
{
	ParmVarDecl *param = cons->getParamDecl(0);
	QualType type = param->getOriginalType();

	int num_params = cons->getNumParams();
	if (num_params != 1)
		return false;

	if (is_isl_type(type) && !is_isl_ctx(type) && is_subclass(type, clazz))
		return true;

	return false;
}

/* Get kind of "method" in "clazz".
 *
 * Given the declaration of a static or member method, returns its kind.
 */
cpp_generator::function_kind cpp_generator::get_method_kind(
	const isl_class &clazz, FunctionDecl *method)
{
	if (is_static(clazz, method))
		return function_kind_static_method;
	else
		return function_kind_member_method;
}
