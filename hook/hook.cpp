#include <Windows.h>

BOOL APIENTRY DllMain (HMODULE, DWORD, LPVOID) { return TRUE; }

#include <easyhook.h>
#include <string>
#include <iostream>
#include <Psapi.h>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <iomanip>
#include <random>

#include <boost/optional.hpp>

#include "asm_x64.hpp"
#include "patching.hpp"
#include "common_types.hpp"
#include "blz_stl.hpp"
#include "renderservice.hpp"

struct callback_test_t
{
  void operator() (RenderRequest::RequestData* request_data, route_arg_3_t* arg3) const
  {
    arg3->message_set_status (204);
    arg3->message_send_and_release();
  }
};
blz::static_function<callback_test_t, void, RenderRequest::RequestData*, route_arg_3_t*> const callback_test;

using _BYTE = unsigned char;
#pragma pack(push, 1)
struct struc_6
{
  const char *name;
  int fileDataID;
  int num_fields_in_file;
  int record_size;
  int num_fields;
  int id_column;
  char sparseTable;
  _BYTE gap19[3];
  int *field_offsets;
  int *field_sizes;
  int *field_types;
  int *field_flags;
  int *field_sizes_in_file;
  int *field_types_in_file;
  int *field_flags_in_file;
  char flags_58_2_1;
  _BYTE gap59[3];
  int table_hash;
  _BYTE gap60[4];
  int layout_hash;
  char flags_68_4_2_1;
  _BYTE gap69[3];
  int nbUniqueIdxByInt;
  int nbUniqueIdxByString;
  _BYTE gap74[4];
  unsigned int *uniqueIdxByInt;
  unsigned int *uniqueIdxByString;
  char bool_88;
  _BYTE gap89[3];
  int column_8C;
  int column_90;
  _BYTE gap94[4];
  bool (__stdcall *sort_func)(_UNKNOWN *, _UNKNOWN *);
  const char *table_name;
  const char **field_names_in_file;
  const char **field_names;
  const char *fk_clause;
  char bool_C0;
};
#pragma pack(pop)
static_assert(sizeof(struc_6) == 0xc1, "");

struct wowdbclient {
  void* vtable;
  using info_t = struc_6;
  info_t* info;
};
//! \note hardcoded to 8.0.1.26095
fun<bool (wowdbclient*, char const*, char const*, unsigned int)> db_load_with_dbgenfallback
  = [] {
    auto a = search_pattern_or_null ("48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 81 EC 40 01 00 00 41 8B D9 49 8B E8 48 8B F2");
    auto b = search_pattern_or_null ("48 89 5C 24 08 57 48 81 EC 50 02 00 00 8B DA");
    if (a && b) throw std::logic_error ("found both patterns");
    // if (!(a || b)) throw std::logic_error ("found both patterns");
    return a | b;
  }();
fun<bool (wowdbclient*, unsigned int)> db_load_with_dbgenfallback_no_override
  = search_pattern_or_null ("48 89 5C 24 08 57 48 81 EC 40 01 00 00 8B DA 48 8B F9 F6 C2 01 0F 84 AE 00 00 00 48 8B 01 4C 8D");
fun<wowdbclient* (wowdbclient*, wowdbclient::info_t*)> sub_1401F3480
  = search_pattern ("40 53 48 83 EC 50 48 89 51 08 48 8D 05 ?? ?? ?? ?? 48 89 01 48 8B D9");
var<char> version_string
  = search_pattern ("52 65 6e 64 65 72 53 65 72 76 69 63 65 20") + strlen ("RenderService ");
// fun<bool()> renderservice_main
  // = search_pattern ("48 8B C4 55 53 56 48 8D A8 ?? ?? FF FF 48 81 EC ?? ?? 00 00");

std::string ea(void const* addr)
{
  auto unrebased = unrebase (addr);
  std::stringstream ss; ss << "0x" << std::hex << unrebased;
  return ss.str();
}

std::string make_type(void const* addr, std::string type)
{
  return "MakeUnkn(" + ea(addr) + ", 1);SetType(" + ea(addr) + ", \"" + type + "\");";
}

struct T {
  std::string type;
  boost::optional<int> bits;
  T (std::string t, boost::optional<int> b = boost::none) : type (t), bits (b) {}
};

T type_to_T (int t, int f)
try
{  
  enum {
    f_maybe_fk = 1,
    f_maybe_compressed = 2, // according to simca
    f_unsigned = 4,
    f_localized = 8,
  };
  
  switch (t)
  {
    case 0:
      switch (f)
      {
        case           0|         0|                 0|         0:
        case           0|         0|                 0|f_maybe_fk:
        case           0|         0|f_maybe_compressed|         0:
        case           0|         0|f_maybe_compressed|f_maybe_fk: 
          return T ( "int", 32);
        case           0|f_unsigned|                 0|         0:
        case           0|f_unsigned|                 0|f_maybe_fk:
        case           0|f_unsigned|f_maybe_compressed|         0:
        case           0|f_unsigned|f_maybe_compressed|f_maybe_fk:
          return T ("uint", 32);
      }
    case 1:                       // may never be an id
      switch (f)
      {
        case           0|         0|                 0|         0: 
          return T ( "int", 64);
      }
    case 2:
      switch (f)
      {
        case           0|         0|                 0|         0:
          return T ("string");
        case f_localized|         0|                 0|         0:
          return T ("locstring");
      }
    case 3:
      switch (f)
      {
        case           0|         0|                 0|         0:
        case           0|         0|f_maybe_compressed|         0:
          return T ("float");
      }
    case 4:
      switch (f)
      {
        case           0|         0|                 0|         0:
        case           0|         0|f_maybe_compressed|         0:
          return T ( "int",  8);
        case           0|f_unsigned|                 0|         0:
        case           0|f_unsigned|f_maybe_compressed|         0:
          return T ("uint",  8);
      }
    case 5:
      switch (f)
      {
        case           0|         0|                 0|         0:
        case           0|         0|                 0|f_maybe_fk:
        case           0|         0|f_maybe_compressed|         0:
        case           0|         0|f_maybe_compressed|f_maybe_fk: 
          return T ( "int", 16);
        case           0|f_unsigned|                 0|         0:
        case           0|f_unsigned|                 0|f_maybe_fk:
        case           0|f_unsigned|f_maybe_compressed|         0:
        case           0|f_unsigned|f_maybe_compressed|f_maybe_fk:
          return T ("uint", 16);
      }
    default:
      throw std::logic_error ("unknown");
  }
}
catch (std::exception const& ex)
{
  std::cerr << "type_to_str (" << t << ", " << f << ") = " << ex.what() << "\n";
  throw;
}

unsigned int get_seed() {
    static std::random_device r;
    static auto seed = r();
    return seed;
}

struct name_gen {
    std::default_random_engine e1;
    name_gen() : e1(get_seed()) {}
std::string operator()() {
    std::uniform_int_distribution<std::uint32_t> uniform_dist;
    return "field_" + std::to_string (uniform_dist (e1));
  }
};

void dump_db_info_ida (wowdbclient* a)
{
  std::string name = a->info->name;
  std::string ida_cmd;
  ida_cmd += "MakeName(" + ea (a) + ", \"db_" + a->info->name + "\");";
  ida_cmd += "MakeName(" + ea (a->info) + ", \"__ZN" + std::to_string(name.size()) + name + "6s_metaE\");";
  ida_cmd += make_type (a->info, "struc_6");
  ida_cmd += make_type (a->info->table_name, "char const a[]");
  ida_cmd += make_type (a->info->field_names_in_file, "char const* a[" + std::to_string (a->info->num_fields_in_file) + "]");
  ida_cmd += make_type (a->info->field_names, "char const* a[" + std::to_string (a->info->num_fields) + "]");
  std::ofstream("E:/git/renderserver/tmp.ida.py", std::ios_base::app) << ida_cmd << "\n";
  
  std::ostringstream dbdss;
  dbdss << "COLUMNS\n";
  
  std::map<std::string, std::pair<int /*type*/, int /*flags*/>> names;

  name_gen namegen1;
  
  for (int i = 0; i < a->info->num_fields_in_file; ++i) {
    auto p = std::make_pair (a->info->field_types_in_file[i], a->info->field_flags_in_file[i]);
    auto name = namegen1();
    names.emplace (name, p);
  }
  if (a->info->num_fields_in_file != a->info->num_fields) {
    auto p = std::make_pair (a->info->field_types[a->info->num_fields_in_file], a->info->field_flags[a->info->num_fields_in_file]);
    auto name = namegen1();
    names.emplace (name, p);
  }
  
  //! \todo Is this the right default?!
  if (a->info->id_column == -1)
    dbdss << "int ID\n";

  for (auto const& name : names)
  {
    auto t = type_to_T (name.second.first, name.second.second);
    if(t.type == "locstring"){
      dbdss << t.type << " " << name.first << "_lang?\n";
    }else{
      dbdss << t.type << " " << name.first << "?\n";
    }
  }
  
  dbdss << "\n";
  auto formatflags = dbdss.flags();
  dbdss << "LAYOUT " << std::setfill ('0') << std::setw(8) << std::uppercase << std::hex << a->info->layout_hash;
  dbdss.flags (formatflags);
  dbdss << "\n";
  dbdss << "BUILD 8.0.1." << version_string.operator->() << "\n";
  if (a->info->sparseTable) dbdss << "COMMENT table is sparse\n";
  //! \todo Is this the right default?!
  if (a->info->id_column == -1)
    dbdss << "$noninline,id$ID<32>\n";
  
  name_gen namegen2;
  for (int i = 0; i < a->info->num_fields_in_file; ++i) {
    auto p = std::make_pair (a->info->field_types_in_file[i], a->info->field_flags_in_file[i]);
    if (a->info->id_column == i)
      dbdss << "$id$";
    if (a->info->column_8C == i){
      dbdss << "$relation$";
      if (a->info->column_90 != i){
        throw std::logic_error ("no unk_90 but there is unk_8C send help");
      }
    }
    dbdss << namegen2();
    auto t = type_to_T (p.first, p.second);
    if(t.type == "locstring"){
      dbdss << "_lang";
    }
    if (t.bits) dbdss << "<" << t.bits.get() << ">";
    if (a->info->field_sizes_in_file[i] != 1)
      dbdss << "[" << a->info->field_sizes_in_file[i] << "]";
    std::set<std::string> comments;
    
    if (!comments.empty())
    {
      dbdss << " // ";
      bool first = true;
      // the one time I wish it was python. ", ".join (comments) is _so_ nice.
      for (auto& comment : comments)
      {
        if (!first) dbdss << ", "; first = false;
        dbdss << comment;
      }
    }
    dbdss << "\n";
  }
  if (a->info->num_fields_in_file != a->info->num_fields) {
    auto i = a->info->num_fields_in_file;
    auto p = std::make_pair (a->info->field_types[i], a->info->field_flags[i]);
    auto t = type_to_T (p.first, p.second);
    dbdss << "$noninline,relation$" << namegen2();
    if(t.type == "locstring"){
      dbdss << "_lang";
    }
    if (t.bits) dbdss << "<" << t.bits.get() << ">";
    if (a->info->field_sizes[i] != 1)
      dbdss << "[" << a->info->field_sizes[i] << "]";
  }
  
  std::ofstream("E:/git/renderserver/tmp-" + name + ".dbd") << dbdss.str();
}

wowdbclient *__fastcall sub_1401F3480_hook(wowdbclient *db, wowdbclient::info_t *info)
try
{
  //std::cerr << "ctor " << info->name << "\n";
  sub_1401F3480 (db, info);
  dump_db_info_ida (db);    

#define to_str_(x) #x
#define to_str(x) to_str_(x)
#define check(_col_) \
  if (info->_col_) { std::string s (info->name); s += " "; s += to_str(_col_); s += "="; s += std::to_string (info->_col_); s += "\n"; std::ofstream("E:/git/renderserver/tmp.log", std::ios_base::app) << s; }
#define check_col(_col_) \
  if (info->_col_ != -1) { std::string s (info->name); s += " "; s += to_str(_col_); s += "="; s += std::to_string (info->_col_); s+=": "; s+=info->field_names[info->_col_]; s += "\n"; std::ofstream("E:/git/renderserver/tmp.log", std::ios_base::app) << s; }
  //check_col (id_column);
  check (flags_58_2_1);
  check (flags_68_4_2_1);
  check (bool_88);
  check (bool_C0);
  
// #define _GAP_ gap94
  // _BYTE* gap = info->_GAP_;
  // for (int i = 0; i < sizeof (info->_GAP_); ++i)
  // {
    // if (info->_GAP_[i]) std::cerr << info->name << " " << to_str(_GAP_) << "[" << i << "]=" << std::to_string (info->_GAP_[i]) << "\n";
  // }

  return db;
}
catch (std::exception const& ex)
{
  std::cerr << "EX: " << ex.what() << "\n";
  return nullptr;
}

void on_inject()
{
  std::ofstream("E:/git/renderserver/tmp.ida.py") << "\n";
  std::ofstream("E:/git/renderserver/tmp.log") << "\n";
  hook (sub_1401F3480, sub_1401F3480_hook);
  std::ofstream("C:/Users/Martin/Documents/GitHub/renderserver/tmp.log") << __LINE__ << "\n";

  // hook ( db_load_with_dbgenfallback
  //      , [] (wowdbclient* a, char const* b, char const* c, unsigned int d)
  //        {
  //          if (std::string (a->info->name) == "AnimationNames") return true;
  //          if (std::string (a->info->name) == "FileDataComplete") return true;
  //          if (std::string (a->info->name) == "FilePaths") return true;
  //          return db_load_with_dbgenfallback (a, b, c, d);
  //        }
  //      );
  //   std::ofstream("C:/Users/Martin/Documents/GitHub/renderserver/tmp.log") << __LINE__ << "\n";

  // if (db_load_with_dbgenfallback_no_override._offset)
  //   hook ( db_load_with_dbgenfallback_no_override
  //        , [] (wowdbclient* a, unsigned int b)
  //          {
  //            if (std::string (a->info->name) == "AnimationNames") return true;
  //            if (std::string (a->info->name) == "FileDataComplete") return true;
  //            if (std::string (a->info->name) == "FilePaths") return true;
  //            return db_load_with_dbgenfallback_no_override (a, b);
  //          }
  //        );
  //        std::ofstream("C:/Users/Martin/Documents/GitHub/renderserver/tmp.log") << __LINE__ << "\n";

  // hook ( CVarRegister
  //      , [] (char const* name, char const* a, unsigned int b, char const* c, bool (*d)(CVar*, char const*, char const*, void *), unsigned int e, bool f, void * g, bool h)
  //        {
  //          return CVarRegister (name, a, b, c, d, e, f, g, h);
  //        }
  //      );

  // hook ( add_route_a
  //      , [] (char const* path, blz::function<void, RenderRequest::RequestData*, route_arg_3_t*> const* fun)
  //        {
  //          std::cerr << "add route a: " << path << "\n";
  //          return add_route_a (path, fun);
  //        }
  //      );
  // hook ( add_route_b
  //      , [] (char const* path, blz::function<void, RenderRequest::RequestData*, route_arg_3_t*> const* fun)
  //        {
  //          std::cerr << "add route b: " << path << "\n";
  //          return add_route_b (path, fun);
  //        }
  //      );
   
  // hook ( renderservice_main
  //      , []
  //        {
  //          add_route ("/test", &callback_test);
  //          return renderservice_main();
  //        }
  //      );
}


extern "C" void __declspec(dllexport) __stdcall NativeInjectionEntryPoint(REMOTE_ENTRY_INFO* inRemoteInfo)
{
  on_inject();
    
  RhWakeUpProcess();
}
 