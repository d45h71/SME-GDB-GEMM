    .text
    .align  4
    .global _row_sme_streaming_vector_bytes

// Streaming vector length is architectural state, independent of nonstreaming
// SVE availability. This leaf changes neither streaming mode nor callee state.
_row_sme_streaming_vector_bytes:
    rdsvl   x0, #1
    ret
