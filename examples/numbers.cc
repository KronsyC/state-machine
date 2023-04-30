#include "../include/builder.h"
using namespace regex_table;

int main() {


  //
  // This state machine reads the numbers one -> ten as strings,
  // and returns their integer forms
  //

  MutableStateMachine<int> state_machine;

  // clang-format off
  state_machine
    .match_sequence("one").commit(1)
    .match_sequence("two").commit(2)
    .match_sequence("three").commit(3)
    .match_sequence("four").commit(4)
    .match_sequence("five").commit(5)
    .match_sequence("six").commit(6)
    .match_sequence("seven").commit(7)
    .match_sequence("eight").commit(8)
    .match_sequence("nine").commit(9)
    .match_sequence("ten").commit(10)
  ;
  // clang-format on
  // state_machine.print_dbg();


  //
  // This regex state machine reads integer literals
  //

  MutableRegex digit;
  digit.match_digit().terminal();

  MutableRegex integer;
  integer
    .match_any_of("123456789")
    .match_many_optionally(digit)
    .terminal()
    .goback()
    .match_any_of("0")
    .terminal()
    .optimize();

  //
  // This regex state machine reads floating point literals
  //
  MutableRegex floatingpoint;
  floatingpoint
    .match(integer)
    .match_any_of(".")
    .match_many_optionally(digit)
    .terminal()
    .optimize();

  floatingpoint.print_dbg();
  return 0;
}
