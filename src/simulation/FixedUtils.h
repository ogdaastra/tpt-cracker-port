#pragma once

#define ELEM_MULT(ELEM) \
	(sim->elements[ELEM].FixedMultiplier)


#define TO_FIXED(VAL, MULT) \
	(int)(VAL * MULT)

#define TO_FIXED_E(VAL, ELEM) \
	TO_FIXED(VAL, ELEM_MULT(ELEM))


#define FROM_FIXED(VAL, MULT) \
	((float)VAL / MULT)

#define FROM_FIXED_E(VAL, ELEM) \
	FROM_FIXED(VAL, ELEM_MULT(ELEM))
