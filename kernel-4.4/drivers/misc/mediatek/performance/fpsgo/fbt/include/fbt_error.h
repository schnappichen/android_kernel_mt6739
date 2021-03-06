
#ifndef __FBT_ERROR_H__
#define __FBT_ERROR_H__

enum FBT_ERROR {
	FBT_OK,
	FBT_ERROR_FAIL,
	FBT_ERROR_OOM,
	FBT_ERROR_OUT_OF_FD,
	FBT_ERROR_FAIL_WITH_LIMIT,
	FBT_ERROR_TIMEOUT,
	FBT_ERROR_CMD_NOT_PROCESSED,
	FBT_ERROR_INVALID_PARAMS,
	FBT_INTENTIONAL_BLOCK
};

#endif
