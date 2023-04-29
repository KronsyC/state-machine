#include "../include/buildtime/builder.h"
using namespace regex_table::build_time;

int main() {
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
   state_machine.print_dbg();
   return 0;
}
