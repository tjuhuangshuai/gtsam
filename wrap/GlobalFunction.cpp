/**
 * @file GlobalFunction.cpp
 *
 * @date Jul 22, 2012
 * @author Alex Cunningham
 */

#include "GlobalFunction.h"
#include "utilities.h"

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

namespace wrap {

using namespace std;

/* ************************************************************************* */
void GlobalFunction::addOverload(bool verbose, const Qualified& overload,
    const ArgumentList& args, const ReturnValue& retVal) {
  if (name.empty())
    name = overload.name;
  else if (overload.name != name)
    throw std::runtime_error(
        "GlobalFunction::addOverload: tried to add overload with name "
            + overload.name + " instead of expected " + name);
  verbose_ = verbose;
  argLists.push_back(args);
  returnVals.push_back(retVal);
  overloads.push_back(overload);
}

/* ************************************************************************* */
void GlobalFunction::matlab_proxy(const std::string& toolboxPath,
    const std::string& wrapperName, const TypeAttributesTable& typeAttributes,
    FileWriter& file, std::vector<std::string>& functionNames) const {

  // cluster overloads with same namespace
  // create new GlobalFunction structures around namespaces - same namespaces and names are overloads
  // map of namespace to global function
  typedef map<string, GlobalFunction> GlobalFunctionMap;
  GlobalFunctionMap grouped_functions;
  for (size_t i = 0; i < overloads.size(); ++i) {
    Qualified overload = overloads.at(i);
    // use concatenated namespaces as key
    string str_ns = qualifiedName("", overload.namespaces);
    ReturnValue ret = returnVals.at(i);
    ArgumentList args = argLists.at(i);

    if (!grouped_functions.count(str_ns))
      grouped_functions[str_ns] = GlobalFunction(name, verbose_);

    grouped_functions[str_ns].argLists.push_back(args);
    grouped_functions[str_ns].returnVals.push_back(ret);
    grouped_functions[str_ns].overloads.push_back(overload);
  }

  size_t lastcheck = grouped_functions.size();
  BOOST_FOREACH(const GlobalFunctionMap::value_type& p, grouped_functions) {
    p.second.generateSingleFunction(toolboxPath, wrapperName, typeAttributes,
        file, functionNames);
    if (--lastcheck != 0)
      file.oss << endl;
  }
}

/* ************************************************************************* */
void GlobalFunction::generateSingleFunction(const std::string& toolboxPath,
    const std::string& wrapperName, const TypeAttributesTable& typeAttributes,
    FileWriter& file, std::vector<std::string>& functionNames) const {

  // create the folder for the namespace
  const Qualified& overload1 = overloads.front();
  createNamespaceStructure(overload1.namespaces, toolboxPath);

  // open destination mfunctionFileName
  string mfunctionFileName = overload1.matlabName(toolboxPath);
  FileWriter mfunctionFile(mfunctionFileName, verbose_, "%");

  // get the name of actual matlab object
  const string matlabQualName = overload1.qualifiedName(".");
  const string matlabUniqueName = overload1.qualifiedName("");
  const string cppName = overload1.qualifiedName("::");

  mfunctionFile.oss << "function varargout = " << name << "(varargin)\n";

  for (size_t overload = 0; overload < argLists.size(); ++overload) {
    const ArgumentList& args = argLists[overload];
    const ReturnValue& returnVal = returnVals[overload];

    const int id = functionNames.size();

    // Output proxy matlab code
    mfunctionFile.oss << "      " << (overload == 0 ? "" : "else");
    argLists[overload].emit_conditional_call(mfunctionFile,
        returnVals[overload], wrapperName, id, true); // true omits "this"

    // Output C++ wrapper code

    const string wrapFunctionName = matlabUniqueName + "_"
        + boost::lexical_cast<string>(id);

    // call
    file.oss << "void " << wrapFunctionName
        << "(int nargout, mxArray *out[], int nargin, const mxArray *in[])\n";
    // start
    file.oss << "{\n";

    returnVal.wrapTypeUnwrap(file);

    // check arguments
    // NOTE: for static functions, there is no object passed
    file.oss << "  checkArguments(\"" << matlabUniqueName
        << "\",nargout,nargin," << args.size() << ");\n";

    // unwrap arguments, see Argument.cpp
    args.matlab_unwrap(file, 0); // We start at 0 because there is no self object

    // call method with default type and wrap result
    if (returnVal.type1.name != "void")
      returnVal.wrap_result(cppName + "(" + args.names() + ")", file,
          typeAttributes);
    else
      file.oss << cppName + "(" + args.names() + ");\n";

    // finish
    file.oss << "}\n";

    // Add to function list
    functionNames.push_back(wrapFunctionName);
  }

  mfunctionFile.oss << "      else\n";
  mfunctionFile.oss
      << "        error('Arguments do not match any overload of function "
      << matlabQualName << "');" << endl;
  mfunctionFile.oss << "      end" << endl;

  // Close file
  mfunctionFile.emit(true);
}

/* ************************************************************************* */

} // \namespace wrap

