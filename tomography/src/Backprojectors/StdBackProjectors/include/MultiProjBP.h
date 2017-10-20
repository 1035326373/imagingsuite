//
// This file is part of the recon library by Anders Kaestner
// (c) 2008 Anders Kaestner
// Distribution is only allowed with the permission of the author.
//
// Revision information
// $Author: kaestner $
// $Date: 2009-07-13 16:48:39 +0200 (Mo, 13 Jul 2009) $
// $Rev: 304 $
//

#ifndef _MULTIPROJBP_H_
#define _MULTIPROJBP_H_

#include <interactors/interactionbase.h>
#include "../include/StdBackProjectorBase.h"

namespace reconstructor{ namespace UnitTests {
	class testBasicReconstructor;
}}
class MultiProjectionBP:
	public StdBackProjectorBase
{
public:
    MultiProjectionBP(kipl::interactors::InteractionBase *interactor=nullptr);
	virtual ~MultiProjectionBP(void);
protected:
	virtual void BackProject();
};

#endif
