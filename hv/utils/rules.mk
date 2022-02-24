LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_DEPS := \
	lib/libc \
	lib/debug

MODULE_SRCS := \
	$(LOCAL_DIR)/rbtree.c 

include make/module.mk
