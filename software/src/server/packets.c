#include <stddef.h>
#include "server/packets.h"

/**
 * Unwrap a wrapped message struct.
 * 
 * @param msg Pointer to a struct message_wrapper.
 * @param msgLen Size of the bytes in the `msg` pointer.
 * @param desiredLen Length of the desired data[] array.
 * @param msgId Desired message ID.
 */
void* receive_wrapped_message(void* msg, size_t msgLen, size_t desiredLen, uint8_t msgId) {
    if (msgLen < MESSAGE_WRAPPER_SIZE + desiredLen) {
        return NULL;
    }

    struct message_wrapper* wrapper = (struct message_wrapper*)msg;

    if (wrapper->id != msgId || wrapper->start != MESSAGE_WRAPPER_START || wrapper->length != desiredLen) {
        return NULL;
    }

    return wrapper->data;
}