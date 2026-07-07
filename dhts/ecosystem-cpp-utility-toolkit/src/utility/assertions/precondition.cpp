#include "nexus/utility/assertions/precondition.h"
#include "nexus/utility/assertions/contract_violation_handler.h"
namespace nexus::utility::assertions {
void Precondition::check(bool c,const char* m,std::source_location l){if(!c)ContractViolationHandler::handleViolation("PRECONDITION","",m,l);}
} // namespace nexus::utility::assertions
