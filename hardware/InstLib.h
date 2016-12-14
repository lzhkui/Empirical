//  This file is part of Empirical, https://github.com/devosoft/Empirical
//  Copyright (C) Michigan State University, 2016.
//  Released under the MIT Software license; see doc/LICENSE
//
//
//  The InstLib class maintains a library of all instructions available to a particular type
//  of virtual CPU, including the functions associated with them, their costs, etc.
//
//  Note: This class is templated on a HARDWARE_TYPE and an INST_TYPE, and thus can be flexible.
//  * HARDWARE_TYPE& is used for the first input for all instruction functions
//  * INST_TYPE must have a working GetID() to transform it into a unique integer.


#ifndef EMP_INSTRUCTION_LIB_H
#define EMP_INSTRUCTION_LIB_H

#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../tools/assert.h"
#include "../tools/errors.h"
#include "../tools/functions.h"
#include "../tools/string_utils.h"

namespace emp {

  // The InstDefinition struct provides the core definition for a possible instruction, linking
  // a name to its description and associate function call.

  template <class HARDWARE_TYPE> class InstDefinition {
  private:
    std::string desc;

    union {
      std::function<bool(HARDWARE_TYPE&)>         call_base;
      std::function<bool(HARDWARE_TYPE&, int)>    call_int;
      std::function<bool(HARDWARE_TYPE&, double)> call_double;
    };

    enum {CALL_NULL, CALL_BASE, CALL_INT, CALL_DOUBLE} call_type;

  public:
    InstDefinition() : call_type(CALL_NULL) { ; }
    InstDefinition(const std::string & in_desc, std::function<bool(HARDWARE_TYPE&)> in_fun)
      : desc(in_desc), call_base(in_fun),   call_type(CALL_BASE) { ; }
    // InstDefinition(const std::string & in_desc, std::function<bool(HARDWARE_TYPE&,int)> in_fun)
    //   : desc(in_desc), call_int(in_fun),    call_type(CALL_INT) { ; }
    InstDefinition(const std::string & in_desc, std::function<bool(HARDWARE_TYPE&,double)> in_fun)
      : desc(in_desc), call_double(in_fun), call_type(CALL_DOUBLE) { ; }
    InstDefinition(const InstDefinition & in_def) : desc(in_def.desc), call_type(in_def.call_type) {
      switch (call_type) {
      case CALL_BASE:   call_base   = in_def.call_base;   break;
      case CALL_INT:    call_int    = in_def.call_int;    break;
      case CALL_DOUBLE: call_double = in_def.call_double; break;
      };
    }
    ~InstDefinition() { ; }

    const InstDefinition & operator=(const InstDefinition & in_def) {
      desc = in_def.desc;
      call_type = in_def.call_type;
      switch (call_type) {
      case CALL_BASE:   call_base   = in_def.call_base;   break;
      case CALL_INT:    call_int    = in_def.call_int;    break;
      case CALL_DOUBLE: call_double = in_def.call_double; break;
      case CALL_NULL:   emp_assert(false);
      };
      return *this;
    }

    const std::string & GetDesc() const { emp_assert(call_type != CALL_NULL); return desc; }

    std::function<bool(HARDWARE_TYPE&)> GetCall(const std::string & in_arg) const {
      emp_assert(call_type != CALL_NULL);
      if (call_type == CALL_BASE)        return call_base;
      if (call_type == CALL_INT)    return std::bind(call_int, _1, std::stoi(in_arg));

      emp_assert(call_type == CALL_DOUBLE);
      return std::bind(call_int, _1, std::stod(in_arg));
    }
  };


  // The InstInfo struct provides detailed information for an instruction implementation
  // active in this instruction set.

  template <typename INST_TYPE> struct InstInfo {
    // User-specified data for each instruction
    std::string name;    // Name of this instruction
    std::string desc;    // Description of this instruction
    int arg_value;       // If used as an argument, what is its value?

    // Auto-generated by InstLib
    char short_name;     // Single character representation of this instruction
    int id;              // Unique ID indicating position of this instruction in the set.
    INST_TYPE prototype; // example of this instruction to be handed out.

    // Arguments
    int cycle_cost;      // CPU Cycle Cost to execute this instruction (default = 1)
    double stability;    // Probability of this cite resisting a mutation (default = 0.0)
    double weight;       // Relative probability of mutating to this instruction (default = 1.0)

    InstInfo(const std::string & _name, const std::string & _desc, int _arg, char _sname, int _id,
             int _cycle_cost, double _stability, double _weight)
      : name(_name), desc(_desc), arg_value(_arg), short_name(_sname), id(_id)
      , prototype(_id, _arg+1, _cycle_cost != 1)
      , cycle_cost(_cycle_cost), stability(_stability), weight(_weight)
    { ; }
  };

  const char inst_char_chart[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
                                   'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
                                   'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
                                   'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
                                   'W', 'X', 'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7',
                                   '8', '9', '!', '@', '$', '%', '^', '&', '*', '_', '=', '-',
                                   '+' };

  template <typename HARDWARE_TYPE, typename INST_TYPE> class InstLib {
  private:
    // Information of the Instructions associated with this InstLib
    // Instruction function pointers are separated out for improved (?) cache performance.
    std::vector< std::function<bool(HARDWARE_TYPE&)> > inst_calls;
    std::vector<InstInfo<INST_TYPE> > inst_info;

    std::map<std::string, int> name_map;
    std::map<char, int> short_name_map;

  public:
    InstLib() { ; }
    ~InstLib() { ; }

    int GetSize() const { return (int) inst_info.size(); }

    // Indexing into an InstLib (by id, name, or symbol) will return  an example instruction
    const INST_TYPE & operator[](int index) {
      emp_assert(index >= 0 && index < (int) inst_info.size());
      return inst_info[index].prototype;
    }
    const INST_TYPE & operator[](std::string name) {
      if (name_map.find(name) == name_map.end()) {
        std::stringstream ss;
        ss << "Trying to access unknown instruction '" << name << "'.  Using default.";
        NotifyError(ss.str());
      }
      return inst_info[name_map[name]].prototype;
    }
    const INST_TYPE & operator[](char symbol) {
      if (short_name_map.find(symbol) == short_name_map.end()) {
        std::stringstream ss;
        ss << "No known instruction associated with symbol '" << symbol << "'.  Using default.";
        NotifyError(ss.str());
      }
      return inst_info[short_name_map[symbol]].prototype;
    }

    inline bool RunInst(HARDWARE_TYPE & hw, int inst_id) const {
      emp_assert(inst_id >= 0 && inst_id < inst_calls.size());
      return inst_calls[inst_id](hw);
    }

    // Add a new instruction to this library.
    InstLib & Add(const std::string & name, const std::string & desc,
                  std::function<bool(HARDWARE_TYPE&)> call, int arg=-1,
                  int cycle_cost=1, double stability=0.0, double weight=1.0) {
      // Make sure we don't have another instruction by this exact name already.
      if (name_map.find(name) != name_map.end()) {
        std::stringstream ss;
        ss << "Adding duplicate instruction name '" << name << "' to instruction library.  Ignoring.";
        NotifyWarning(ss.str());
        return *this;
      }

      // Generate ID information for this new instruction.
      const int next_id = (int) inst_info.size();  // The ID number of this new instruction.
      const int char_id = std::min(next_id, 72);   // We only have 72 chars, so additional are "+"
      const char next_char = inst_char_chart[char_id];

      // Save this function call separately from everything else for fast lookup.
      inst_calls.push_back(call);

      // Save all of the other information
      inst_info.push_back( InstInfo<INST_TYPE>(name, desc, arg, next_char, next_id,
                                               cycle_cost, stability, weight) );

      // Make sure we can look up this instruction quickly by name or char ID.
      name_map[name] = next_id;
      if (next_id == char_id) short_name_map[next_char] = next_id;

      return *this;
    }


    // Retrieve information about each instruction.
    const std::string & GetName(const INST_TYPE & inst) const { return inst_info[inst.GetID()].name; }
    char GetShortName(const INST_TYPE & inst) const { return inst_info[inst.GetID()].short_name; }
    int GetCycleCost(const INST_TYPE & inst) const { return inst_info[inst.GetID()].cycle_cost; }
    int GetID(const INST_TYPE & inst) const { return inst_info[inst.GetID()].id; }

    // Convert an INST_TYPE into a single character (only works perfectly if inst lib size < 72)
    char AsChar(const INST_TYPE & inst) const { return inst_info[inst.GetID()].short_name; }

    //Convert an instruction vector into a series of characters.
    std::string AsString(const std::vector<INST_TYPE> & inst_vector) const {
      const int vector_size = inst_vector.GetSize();
      std::string out_string(vector_size, ' ');
      for (int i = 0; i < vector_size; i++) {
        out_string[i] = ToChar(inst_vector[i]);
      }
      // @CAO Should we do something here to facilitate move sematics?
      return out_string;
    }

    // The following function will load a specified instruction into this instruction library.
    //
    // The incoming string should look like:
    //  inst_name:inst_specs arg1=value arg2=value ...
    //
    // The instruction name (inst_name) is the built-in name for the instruction (i.e., "Nop",
    // "Inc", or "Divide") and can be followed by a colon and any specifications needed for the
    // instruction.
    //
    // Other arguments in an instruction definition specify additional details for how this
    // instruction should behave in non-standard ways.  They include:
    //   cycle_cost - The number of CPU cycles that must be spent to execute this instruction.
    //       (type=int; range=1+; default=1)
    //   mod_id - Mark this instruction as a modifier for other instructions
    //       (type=int; range=0+; default=non-modifier)
    //   name - Have this instruction be referred to by a custom name.
    //   stability - The additional probability of this instruction "resisting" an error.
    //       (type=double; range=0.0-1.0; default = 0.0)
    //   weight - The relative probability of errors shifting to this instruction.
    //       (type=double; range=0.0+; default = 1.0)
    //
    // For example...
    //
    //    PushValue:3 name=Push-3 stability=1.0 weight=0.01
    //
    // ...would create an instruction called "Push-3" that pushes the value 3 onto the top of
    // a stack.  It would also be unlikely to mutate to (low weight), but impossible to mutate
    // away from once it was in place (max stability).  It would cost 1 CPU cycle to execute
    // and not modify other instructions since neither of those values were set.


    bool LoadInst(std::string inst_info)
    {
      // Determine the instruction name.
      compress_whitespace(inst_info);
      std::string full_name = string_pop_word(inst_info);  // Full name of inst     eg: PushValue:3
      std::string name_spec = full_name;                   // Specs at end of name  eg: 3
      std::string name_base = string_pop(name_spec, ':');  // Base name of inst     eg: PushValue

      // Set all of the arguments to their defaults.
      int cycle_cost = 1;                  // How many CPU cycles should this instruction cost?
      int mod_id = -1;                     // Should this instruction modify others?
      std::string name_final = full_name;  // What name should this inst be stored under?
      double stability = 0.0;              // How resistant is this instruction to errors?
      double weight = 1.0;                 // How likely will an error change to this instruction?

      // Collect additional arguments.
      while(inst_info.size() > 0) {
        std::string arg_info = string_pop_word(inst_info);  // Value assigned to (rhs)
        std::string arg_name = string_pop(arg_info, '=');   // Variable name.

        if (arg_name == "cycle_cost") {
          cycle_cost = std::stoi(arg_info);
          if (cycle_cost < 1) {
            std::stringstream ss;
            ss << "Trying to set '" << full_name << "' cycle_cost to " << cycle_cost
               << ". Using minimum of 1 instead.";
            NotifyError(ss.str());
            cycle_cost = 1;
          }
        }
        else if (arg_name == "mod_id") {
          mod_id = std::stoi(arg_info);
        }
        else if (arg_name == "name") {
          if (arg_info.size() == 0) {
            std::stringstream ss;
            ss << "Trying to set '" << full_name << "' to have no name.  Ignoring.";
            NotifyError(ss.str());
          }
          else {
            name_final = arg_info;
          }
        }
        else if (arg_name == "stability") {
          stability = std::stoi(arg_info);
          if (stability < 0.0 || stability > 1.0) {
            std::stringstream ss;
            ss << "Trying to set '" << full_name << "' stability to " << stability;
            stability = ToRange(stability, 0.0, 1.0);
            ss << ". Using extreme of " << stability << " instead.";
            NotifyError(ss.str());
          }
        }
        else if (arg_name == "weight") {
          weight = std::stod(arg_info);
          if (weight < 0.0) {
            std::stringstream ss;
            ss << "Trying to set '" << full_name << "' cycle_cost to " << weight
               << ". Using minimum of 0 instead.";
            NotifyError(ss.str());
            weight = 0.0;
          }
        }
        else {
          std::stringstream ss;
          ss << "Unknown argument '" << arg_name << "'.  Ignoring.";
          NotifyError(ss.str());
        }
      }

      const auto & inst_defs = HARDWARE_TYPE::GetInstDefs();
      if (inst_defs.find(name_base) == inst_defs.end()) {
        std::stringstream ss;
        ss << "Failed to find instruction '" << name_base << "'.  Ignoring.";
        NotifyError(ss.str());

        return false;
      }

      const auto & cur_def = inst_defs.at(name_base);

      Add(name_final, cur_def.GetDesc(), cur_def.GetCall(name_spec),
          mod_id, cycle_cost, stability, weight);

      return true;
    }

    void LoadDefaults() {
      auto default_insts = HARDWARE_TYPE::GetDefaultInstructions();
      for (const std::string & inst_name : default_insts) {
        LoadInst(inst_name);
      }
    }


  };
};

#endif
