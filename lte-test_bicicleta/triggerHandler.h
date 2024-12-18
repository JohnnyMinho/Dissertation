#ifndef ARTERY_CASERVICE_H_
#define ARTERY_CASERVICE_H_

#include "artery/application/ItsG5BaseService.h"

namespace artery
{

class triggerHandler : public ItsG5BaseService
{
    public:
        virtual void trigger(); // Declare trigger as virtual for overriding
}

#endif