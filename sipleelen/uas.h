#ifndef SIPLEELEN_UAS_H
#define SIPLEELEN_UAS_H

#ifdef __cplusplus
extern "C" {
#endif

// #include "ua.h"
struct SIPLeelen;


__attribute__((nonnull))
/**
 * @memberof SIPLeelen
 * @brief Add callbacks for SIP transaction processing.
 *
 * @param self LEELEN2SIP object.
 * @return 0.
 */
int SIPLeelen_set_uas_callbacks (struct SIPLeelen *self);


#ifdef __cplusplus
}
#endif

#endif /* SIPLEELEN_UAS_H */
