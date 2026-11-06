// SPDX-License-Identifier: GPL-3.0-or-later
#include "el.h"
#include "el/EvaluationContext.h"
#include "el/Interpolate.h"
#include "el/ParseExpression.h"
#include "el/VariableStore.h"
#include "kd/cmd_utils.h"
#include "kd/string_compare.h"
#include "kd/string_utils.h"
#include <stdexcept>
namespace ftebroom::profile {
namespace {
tb::el::Value value(const Json& j) {
 using namespace tb::el;
 if(j.is_null())return Value::Null;
 if(j.is_boolean())return Value{j.get<bool>()};
 if(j.is_number())return Value{j.get<double>()};
 if(j.is_string())return Value{j.get<std::string>()};
 if(j.is_array()){ArrayType a;for(const auto& v:j)a.push_back(value(v));return Value{std::move(a)};}
 MapType m;for(auto it=j.begin();it!=j.end();++it)m.emplace(it.key(),value(it.value()));return Value{std::move(m)};
}
Json jsonValue(const tb::el::Value& v, tb::el::EvaluationContext& ctx) {
 using namespace tb::el;
 switch(v.type()) {
 case ValueType::Boolean:return v.booleanValue(ctx);
 case ValueType::Number:return v.numberValue(ctx);
 case ValueType::String:return v.stringValue(ctx);
 case ValueType::Array:{auto j=Json::array();for(const auto& e:v.arrayValue(ctx))j.push_back(jsonValue(e,ctx));return j;}
 case ValueType::Map:{auto j=Json::object();for(const auto& e:v.mapValue(ctx))j[e.first]=jsonValue(e.second,ctx);return j;}
 case ValueType::Null:return nullptr;
 default:throw std::runtime_error("Configuration contains an undefined or non-serializable EL value.");
 }
}
}
std::string interpolate(const std::string& text,const Json& variables) {
 tb::el::VariableTable table;
 for(auto it=variables.begin();it!=variables.end();++it)table.set(it.key(),value(it.value()));
 std::string out;std::string error;
 tb::el::interpolate(table,text).transform([&](const auto& s){out=s;}).transform_error([&](const auto& e){error=e.msg;});
 if(!error.empty())throw std::runtime_error(error);return out;
}
Json parseConfig(const std::string& text) {
 Json result;std::string error;
 tb::el::parseExpression(tb::el::ParseMode::Strict,text).transform([&](const auto& expression){
  auto r=tb::el::withEvaluationContext([&](auto& ctx){return jsonValue(expression.evaluate(ctx),ctx);});
  r.transform([&](const auto& j){result=j;}).transform_error([&](const auto& e){error=e.msg;});
 }).transform_error([&](const auto& e){error=e.msg;});
 if(!error.empty())throw std::runtime_error(error);return result;
}
std::vector<std::string> parseArguments(const std::string& text){return kdl::cmd_parse_args(text);}
std::vector<std::string> splitMods(const std::string& mods){return kdl::str_split(mods,";");}
bool filenameMatches(const std::string& name,const std::string& pattern){return kdl::ci::str_matches_glob(name,pattern);}
}
