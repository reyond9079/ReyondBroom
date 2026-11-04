// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <cstdlib>
namespace ftebroom {
struct FgdModelRule{std::string condition,path,skin="=0",frame="=0",scale;};
// A bounded, data-only subset of TrenchBroom model expressions. No functions,
// file access or code execution. Fields are encoded as $property / =literal.
class FgdModels {
 struct Token{std::string text;bool quoted=false;};std::vector<Token> tokens;size_t at=0;
 std::string peek()const{return at<tokens.size()?tokens[at].text:"";}
 Token take(){if(at>=tokens.size())throw std::runtime_error("Incomplete FGD model expression");return tokens[at++];}
 bool accept(const char* s){if(peek()!=s)return false;at++;return true;}
 void expect(const char* s){if(!accept(s))throw std::runtime_error(std::string("Expected ")+s+" in FGD model expression");}
 static int precedence(const std::string& op){if(op=="||")return 1;if(op=="&&")return 2;if(op=="|")return 3;if(op=="^")return 4;if(op=="&")return 5;if(op=="=="||op=="!=")return 6;if(op=="<"||op==">"||op=="<="||op==">=")return 7;return 0;}
 std::string operand(int depth=0){if(depth>64)throw std::runtime_error("FGD model expression is too deeply nested");if(accept("(")){auto s=expression(1,depth+1);expect(")");return s;}if(accept("!"))return operand(depth+1)+" !";if(accept("-"))return "0 "+operand(depth+1)+" -";auto t=take();if(t.quoted)return std::to_string(std::strtod(t.text.c_str(),nullptr));return (std::isalpha((unsigned char)t.text[0])||t.text[0]=='_'?"$":"")+t.text;}
 std::string expression(int minimum=1,int depth=0){auto left=operand(depth);for(;;){auto op=peek();int p=precedence(op);if(p<minimum)break;take();left+=" "+expression(p+1,depth+1)+" "+op;}return left;}
 std::string value(){auto t=take();bool field=!t.quoted&&!t.text.empty()&&(std::isalpha((unsigned char)t.text[0])||t.text[0]=='_');return (field?"$":"=")+t.text;}
 FgdModelRule object(){FgdModelRule r;expect("{");while(!accept("}")){auto key=take().text;expect(":");auto v=value();if(key=="path")r.path=v;if(key=="skin")r.skin=v;if(key=="frame")r.frame=v;if(key=="scale")r.scale=v;if(peek()!="}")expect(",");}return r;}
 public:
 explicit FgdModels(const std::string& source){for(size_t i=0;i<source.size();){unsigned char c=source[i];if(std::isspace(c)){i++;continue;}Token t;if(c=='"'){t.quoted=true;i++;while(i<source.size()&&source[i]!='"'){if(source[i]=='\\'&&i+1<source.size())i++;t.text+=source[i++];}if(i==source.size())throw std::runtime_error("Unclosed FGD model string");i++;}else if(i+1<source.size()&&(source.substr(i,2)=="->"||source.substr(i,2)=="=="||source.substr(i,2)=="!="||source.substr(i,2)=="<="||source.substr(i,2)==">="||source.substr(i,2)=="&&"||source.substr(i,2)=="||")){t.text=source.substr(i,2);i+=2;}else if(std::string("{}():,><=!&|^+-").find(c)!=std::string::npos)t.text=source[i++];else{while(i<source.size()&&!std::isspace((unsigned char)source[i])&&std::string("{}():,><=!&|^+-\"").find(source[i])==std::string::npos)t.text+=source[i++];}tokens.push_back(t);if(tokens.size()>8192)throw std::runtime_error("FGD model expression exceeds token budget");}}
 std::vector<FgdModelRule> read(){std::vector<FgdModelRule> rules;for(size_t i=0;i+1<tokens.size();i++)if(!tokens[i].quoted&&!tokens[i+1].quoted&&(tokens[i].text=="="||tokens[i].text=="!"||tokens[i].text=="<"||tokens[i].text==">")&&tokens[i+1].text=="="){tokens[i].text+="=";tokens.erase(tokens.begin()+i+1);}if(tokens.empty())return rules;if(tokens.front().quoted){FgdModelRule r;r.path=value();rules.push_back(r);return rules;}if(tokens.size()>1&&tokens[0].text=="{"&&tokens[1].text=="{"){take();take();while(peek()!="}"){if(at>=tokens.size())throw std::runtime_error("Unclosed FGD model cases");std::string condition;if(peek()!="{"&&!tokens[at].quoted){condition=expression();expect("->");}FgdModelRule r;if(at>=tokens.size())throw std::runtime_error("Missing FGD model case result");if(tokens[at].quoted)r.path=value();else r=object();r.condition=condition;rules.push_back(r);if(rules.size()>256)throw std::runtime_error("Too many FGD model cases");if(!accept(","))break;}expect("}");expect("}");}else rules.push_back(object());return rules;}
};
inline std::string fgdValue(const std::string& value,const std::map<std::string,std::string>& properties){if(value.empty())return {};if(value[0]=='=')return value.substr(1);auto it=properties.find(value.substr(1));return it==properties.end()?"":it->second;}
inline bool fgdCondition(const std::string& code,const std::map<std::string,std::string>& properties){if(code.empty())return true;std::istringstream in(code);std::vector<double> stack;std::string token;while(in>>token){if(stack.size()>128)throw std::runtime_error("FGD condition stack overflow");if(token=="!"){if(stack.empty())return false;stack.back()=!stack.back();continue;}if(token=="&"||token=="|"||token=="^"||token=="&&"||token=="||"||token=="=="||token=="!="||token=="<"||token==">"||token=="<="||token==">="||token=="-"){if(stack.size()<2)return false;double b=stack.back();stack.pop_back();double a=stack.back();if(token=="&")a=int(a)&int(b);else if(token=="|")a=int(a)|int(b);else if(token=="^")a=int(a)^int(b);else if(token=="&&")a=a&&b;else if(token=="||")a=a||b;else if(token=="==")a=a==b;else if(token=="!=")a=a!=b;else if(token=="<")a=a<b;else if(token==">")a=a>b;else if(token=="<=")a=a<=b;else if(token==">=")a=a>=b;else a-=b;stack.back()=a;}else{auto value=token[0]=='$'?fgdValue(token,properties):token;stack.push_back(std::strtod(value.c_str(),nullptr));}}return stack.size()==1&&stack[0]!=0;}
}
