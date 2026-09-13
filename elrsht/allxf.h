const uint8_t allxf_ht_message_header = 0x5A;
const uint8_t allxf_ht_message_footer = 0xA5;

// From Ardupilot, https://github.com/Hwurzburg/ardupilot/blob/5a4f7b50058825af34de3f0f73198a7733186df5/libraries/AP_Mount/AP_Mount_CADDX.h#L65
typedef struct __attribute__  ((packed)) {
	uint8_t  mode:3;        // Gimbal Mode [0~7] [Only 0 1 2 modes are supported for the time being]
	int16_t  sensitivity:5; // Stabilization sensibility [-15~15]
	uint8_t  reserved:4;    
	int32_t  roll:12;       // Roll angle [-2048~2047] => [-180~180]
	int32_t  tilt:12;       // Pitch angle [-2048~2047] => [-180~180]
	int32_t  pan:12;        // Yaw angle [-2048~2047] => [-180~180]
	uint8_t  crch;          // Data validation H
	uint8_t  crcl;          // Data validation L
} allxf_htMessage_t;

typedef enum {
  STATE_NEW_MESSAGE,
  STATE_RECEIVING,
  STATE_WAIT_FOOTER
} allxf_htState_t;