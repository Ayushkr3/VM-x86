// keyboard.h
#pragma once
#include <stdint.h>
#include <queue>
#include <functional>

// 8042 Status Register bits
#define KBD_STATUS_OBF      0x01  // Output buffer full (data ready to read)
#define KBD_STATUS_IBF      0x02  // Input buffer full (busy)
#define KBD_STATUS_SYS      0x04  // System flag
#define KBD_STATUS_CD       0x08  // Command/Data (1=command, 0=data)
#define KBD_STATUS_KEYLOCK  0x10  // Keyboard locked
#define KBD_STATUS_AUXOBF   0x20  // Aux output buffer full (mouse)
#define KBD_STATUS_TIMEOUT  0x40  // Timeout error
#define KBD_STATUS_PARITY   0x80  // Parity error

// 8042 Commands (port 0x64 write)
#define KBD_CMD_READ_CCB        0x20  // Read controller command byte
#define KBD_CMD_WRITE_CCB       0x60  // Write controller command byte
#define KBD_CMD_DISABLE_AUX     0xA7  // Disable auxiliary port
#define KBD_CMD_ENABLE_AUX      0xA8  // Enable auxiliary port
#define KBD_CMD_TEST_AUX        0xA9  // Test auxiliary port
#define KBD_CMD_SELF_TEST       0xAA  // Controller self test
#define KBD_CMD_TEST_KBD        0xAB  // Test keyboard port
#define KBD_CMD_DISABLE_KBD     0xAD  // Disable keyboard
#define KBD_CMD_ENABLE_KBD      0xAE  // Enable keyboard
#define KBD_CMD_READ_INPUT      0xC0  // Read input port
#define KBD_CMD_READ_OUTPUT     0xD0  // Read output port
#define KBD_CMD_WRITE_OUTPUT    0xD1  // Write output port
#define KBD_CMD_WRITE_AUX       0xD4  // Write to aux device
#define KBD_CMD_PULSE_OUTPUT    0xF0  // Pulse output port bits

// Keyboard commands (sent to port 0x60)
#define KBD_DEV_CMD_RESET       0xFF
#define KBD_DEV_CMD_RESEND      0xFE
#define KBD_DEV_CMD_SET_DEFAULT 0xF6
#define KBD_DEV_CMD_DISABLE     0xF5
#define KBD_DEV_CMD_ENABLE      0xF4
#define KBD_DEV_CMD_SET_RATE    0xF3
#define KBD_DEV_CMD_GET_ID      0xF2
#define KBD_DEV_CMD_SET_LEDS    0xED
#define KBD_DEV_CMD_ECHO        0xEE
#define KBD_DEV_CMD_SCAN_MODE   0xF0

// Keyboard responses
#define KBD_RESP_ACK        0xFA
#define KBD_RESP_RESEND     0xFE
#define KBD_RESP_BAT_OK     0xAA  // Basic Assurance Test passed

// CCB bits (Controller Command Byte)
#define KBD_CCB_INT         0x01  // Enable keyboard interrupt (IRQ1)
#define KBD_CCB_AUX_INT     0x02  // Enable aux interrupt (IRQ12)
#define KBD_CCB_SYS         0x04  // System flag
#define KBD_CCB_DISABLE_KBD 0x10  // Disable keyboard
#define KBD_CCB_DISABLE_AUX 0x20  // Disable aux
#define KBD_CCB_TRANSLATE   0x40  // Scancode translation (set2->set1)

using RaiseIRQ_kbd = std::function<void(int)>;

class PS2Keyboard {
public:
    // Output buffer — data going TO CPU (read from 0x60)
    std::queue<uint8_t> output_buf;

    // Input buffer — data FROM CPU (written to 0x60/0x64)
    std::queue<uint8_t> input_buf;

    uint8_t status_reg;
    uint8_t ccb;            // Controller Command Byte
    uint8_t last_cmd;       // Last command written to 0x64
    bool    expecting_data; // Waiting for data byte after command
    bool    kbd_disabled;
    bool    aux_disabled;
    bool    expecting_kbd_data; // waiting for keyboard device command data

    uint8_t kbd_leds;
    uint8_t scan_mode;      // 1, 2, or 3
    bool    expecting_scanmode;
    bool    expecting_rate;
    bool    expecting_led;

    RaiseIRQ_kbd raise_irq;

    PS2Keyboard(RaiseIRQ_kbd irq_fn) :
        status_reg(KBD_STATUS_SYS | KBD_STATUS_KEYLOCK),
        ccb(KBD_CCB_INT | KBD_CCB_TRANSLATE | KBD_CCB_SYS),
        last_cmd(0),
        expecting_data(false),
        kbd_disabled(false),
        aux_disabled(true),
        kbd_leds(0),
        scan_mode(2),
        expecting_scanmode(false),
        expecting_rate(false),
        expecting_led(false),
        expecting_kbd_data(false),
        raise_irq(irq_fn)
    {}

    // Push byte into output buffer → CPU reads it from 0x60
    void push_output(uint8_t val) {
        output_buf.push(val);
        status_reg |= KBD_STATUS_OBF;
    }

    // Raise IRQ1 (keyboard interrupt)
    void trigger_irq() {
        if (ccb & KBD_CCB_INT) {
            raise_irq(1);
        }
    }

    // CPU reads port 0x60
    uint8_t read_data() {
        if (output_buf.empty()) return 0x00;
        uint8_t val = output_buf.front();
        output_buf.pop();
        if (output_buf.empty())
            status_reg &= ~KBD_STATUS_OBF;
        return val;
    }

    // CPU reads port 0x64
    uint8_t read_status() {
        return status_reg;
    }

    // CPU writes port 0x64 (command)
    void write_command(uint8_t cmd) {
        last_cmd = cmd;
        expecting_data = false;

        switch (cmd) {
        case KBD_CMD_READ_CCB:
            push_output(ccb);
            trigger_irq();
            break;

        case KBD_CMD_WRITE_CCB:
            expecting_data = true;  // next write to 0x60 is CCB value
            break;

        case KBD_CMD_DISABLE_KBD:
            kbd_disabled = true;
            ccb |= KBD_CCB_DISABLE_KBD;
            break;

        case KBD_CMD_ENABLE_KBD:
            kbd_disabled = false;
            ccb &= ~KBD_CCB_DISABLE_KBD;
            break;

        case KBD_CMD_DISABLE_AUX:
            aux_disabled = true;
            ccb |= KBD_CCB_DISABLE_AUX;
            break;

        case KBD_CMD_ENABLE_AUX:
            aux_disabled = false;
            ccb &= ~KBD_CCB_DISABLE_AUX;
            break;

        case KBD_CMD_SELF_TEST:
            // Return 0x55 = test passed
            push_output(0x55);
            status_reg |= KBD_STATUS_SYS;
            ccb |= KBD_CCB_SYS;
            trigger_irq();
            break;

        case KBD_CMD_TEST_KBD:
            // Return 0x00 = no error
            push_output(0x00);
            trigger_irq();
            break;

        case KBD_CMD_TEST_AUX:
            // Return 0x00 = no error
            push_output(0x00);
            trigger_irq();
            break;

        case KBD_CMD_READ_OUTPUT:
            // Output port: bit1=A20, bit0=reset
            push_output(0xCF);
            trigger_irq();
            break;

        case KBD_CMD_WRITE_OUTPUT:
            expecting_data = true;
            break;

        case KBD_CMD_WRITE_AUX:
            expecting_data = true;
            break;

        case KBD_CMD_READ_INPUT:
            // Input port: keyboard not locked, aux present
            push_output(0xB0);
            trigger_irq();
            break;

        default:
            // Pulse output port (0xF0-0xFF)
            if ((cmd & 0xF0) == 0xF0) {
                if (!(cmd & 0x01)) {
                    // bit 0 = reset line
                    // system reset requested — handle if needed
                    printf("[KBD] System reset requested!\n");
                }
            }
            break;
        }
    }

    // CPU writes port 0x60 (data or keyboard command)
    void write_data(uint8_t val) {
        status_reg &= ~KBD_STATUS_IBF;

        if (expecting_data) {
            // This is data for a previous 0x64 command
            expecting_data = false;
            handle_controller_data(val);
            return;
        }

        // Otherwise it's a keyboard device command
        handle_kbd_command(val);
    }

    // Handle data byte for controller commands
    void handle_controller_data(uint8_t val) {
        switch (last_cmd) {
        case KBD_CMD_WRITE_CCB:
            ccb = val;
            kbd_disabled = (ccb & KBD_CCB_DISABLE_KBD) != 0;
            aux_disabled = (ccb & KBD_CCB_DISABLE_AUX) != 0;
            printf("[KBD] CCB written: 0x%02X\n", val);
            break;

        case KBD_CMD_WRITE_OUTPUT:
            // bit 1 = A20 gate
            // bit 0 = CPU reset
            printf("[KBD] Output port written: 0x%02X\n", val);
            break;

        case KBD_CMD_WRITE_AUX:
            // Data for mouse — ACK it
            push_output(KBD_RESP_ACK);
            trigger_irq();
            break;

        default:
            break;
        }
    }

    // Handle keyboard device commands
    void handle_kbd_command(uint8_t val) {
        if (expecting_scanmode) {
            expecting_scanmode = false;
            if (val == 0) {
                // Query current mode
                push_output(KBD_RESP_ACK);
                push_output(scan_mode);
            }
            else {
                scan_mode = val;
                push_output(KBD_RESP_ACK);
            }
            trigger_irq();
            return;
        }

        if (expecting_rate) {
            expecting_rate = false;
            push_output(KBD_RESP_ACK);
            trigger_irq();
            return;
        }

        if (expecting_led) {
            expecting_led = false;
            kbd_leds = val & 0x07;
            push_output(KBD_RESP_ACK);
            trigger_irq();
            printf("[KBD] LEDs: scroll=%d num=%d caps=%d\n",
                kbd_leds & 1, (kbd_leds >> 1) & 1, (kbd_leds >> 2) & 1);
            return;
        }

        switch (val) {
        case KBD_DEV_CMD_RESET:
            push_output(KBD_RESP_ACK);
            push_output(KBD_RESP_BAT_OK);  // BAT passed
            trigger_irq();
            break;

        case KBD_DEV_CMD_ENABLE:
            kbd_disabled = false;
            push_output(KBD_RESP_ACK);
            trigger_irq();
            break;

        case KBD_DEV_CMD_DISABLE:
            kbd_disabled = true;
            push_output(KBD_RESP_ACK);
            trigger_irq();
            break;

        case KBD_DEV_CMD_SET_DEFAULT:
            scan_mode = 2;
            kbd_leds = 0;
            push_output(KBD_RESP_ACK);
            trigger_irq();
            break;

        case KBD_DEV_CMD_GET_ID:
            push_output(KBD_RESP_ACK);
            push_output(0xAB);  // keyboard ID byte 1
            push_output(0x83);  // keyboard ID byte 2
            trigger_irq();
            break;

        case KBD_DEV_CMD_SET_LEDS:
            push_output(KBD_RESP_ACK);
            expecting_led = true;
            trigger_irq();
            break;

        case KBD_DEV_CMD_SET_RATE:
            push_output(KBD_RESP_ACK);
            expecting_rate = true;
            trigger_irq();
            break;

        case KBD_DEV_CMD_SCAN_MODE:
            push_output(KBD_RESP_ACK);
            expecting_scanmode = true;
            trigger_irq();
            break;

        case KBD_DEV_CMD_ECHO:
            push_output(0xEE);  // Echo response
            trigger_irq();
            break;

        case KBD_DEV_CMD_RESEND:
            // Resend last byte — simplified
            push_output(KBD_RESP_ACK);
            trigger_irq();
            break;

        default:
            push_output(KBD_RESP_ACK);
            trigger_irq();
            break;
        }
    }

    // Send a scancode to CPU (call this when key pressed/released)
    void send_scancode(uint8_t scancode) {
        if (kbd_disabled) return;
        push_output(scancode);
        trigger_irq();
    }

    // Send multiple bytes (e.g. extended keys E0 xx)
    void send_scancodes(std::initializer_list<uint8_t> codes) {
        if (kbd_disabled) return;
        for (uint8_t c : codes) {
            output_buf.push(c);
        }
        status_reg |= KBD_STATUS_OBF;
        trigger_irq();
    }
};

// Scancode Set 1 (what BIOS/DOS expects after translation)
namespace Scancode1 {
    // Make codes (key press)
    constexpr uint8_t ESC = 0x01;
    constexpr uint8_t KEY_1 = 0x02;
    constexpr uint8_t KEY_2 = 0x03;
    constexpr uint8_t KEY_3 = 0x04;
    constexpr uint8_t KEY_4 = 0x05;
    constexpr uint8_t KEY_5 = 0x06;
    constexpr uint8_t KEY_6 = 0x07;
    constexpr uint8_t KEY_7 = 0x08;
    constexpr uint8_t KEY_8 = 0x09;
    constexpr uint8_t KEY_9 = 0x0A;
    constexpr uint8_t KEY_0 = 0x0B;
    constexpr uint8_t MINUS = 0x0C;
    constexpr uint8_t EQUALS = 0x0D;
    constexpr uint8_t BACKSPACE = 0x0E;
    constexpr uint8_t TAB = 0x0F;
    constexpr uint8_t Q = 0x10;
    constexpr uint8_t W = 0x11;
    constexpr uint8_t E = 0x12;
    constexpr uint8_t R = 0x13;
    constexpr uint8_t T = 0x14;
    constexpr uint8_t Y = 0x15;
    constexpr uint8_t U = 0x16;
    constexpr uint8_t I = 0x17;
    constexpr uint8_t O = 0x18;
    constexpr uint8_t P = 0x19;
    constexpr uint8_t ENTER = 0x1C;
    constexpr uint8_t LCTRL = 0x1D;
    constexpr uint8_t A = 0x1E;
    constexpr uint8_t S = 0x1F;
    constexpr uint8_t D = 0x20;
    constexpr uint8_t F = 0x21;
    constexpr uint8_t G = 0x22;
    constexpr uint8_t H = 0x23;
    constexpr uint8_t J = 0x24;
    constexpr uint8_t K = 0x25;
    constexpr uint8_t L = 0x26;
    constexpr uint8_t LSHIFT = 0x2A;
    constexpr uint8_t Z = 0x2C;
    constexpr uint8_t X = 0x2D;
    constexpr uint8_t C = 0x2E;
    constexpr uint8_t V = 0x2F;
    constexpr uint8_t B = 0x30;
    constexpr uint8_t N = 0x31;
    constexpr uint8_t M = 0x32;
    constexpr uint8_t RSHIFT = 0x36;
    constexpr uint8_t LALT = 0x38;
    constexpr uint8_t SPACE = 0x39;
    constexpr uint8_t CAPSLOCK = 0x3A;
    constexpr uint8_t F1 = 0x3B;
    constexpr uint8_t F2 = 0x3C;
    constexpr uint8_t F3 = 0x3D;
    constexpr uint8_t F4 = 0x3E;
    constexpr uint8_t F5 = 0x3F;
    constexpr uint8_t F6 = 0x40;
    constexpr uint8_t F7 = 0x41;
    constexpr uint8_t F8 = 0x42;
    constexpr uint8_t F9 = 0x43;
    constexpr uint8_t F10 = 0x44;
    constexpr uint8_t F11 = 0x57;
    constexpr uint8_t F12 = 0x58;
    //constexpr uint8_t DELETE = 0x53;
    constexpr uint8_t UP = 0x48;
    constexpr uint8_t DOWN = 0x50;
    constexpr uint8_t LEFT = 0x4B;
    constexpr uint8_t RIGHT = 0x4D;

    // Break code = make code | 0x80
    constexpr uint8_t BREAK(uint8_t make) { return make | 0x80; }

    // Extended keys (preceded by 0xE0)
    namespace Extended {
        constexpr uint8_t PREFIX = 0xE0;
        constexpr uint8_t RCTRL = 0x1D;
        constexpr uint8_t RALT = 0x38;
        constexpr uint8_t HOME = 0x47;
        constexpr uint8_t UP = 0x48;
        constexpr uint8_t PGUP = 0x49;
        constexpr uint8_t LEFT = 0x4B;
        constexpr uint8_t RIGHT = 0x4D;
        constexpr uint8_t END = 0x4F;
        constexpr uint8_t DOWN = 0x50;
        constexpr uint8_t PGDN = 0x51;
        constexpr uint8_t INSERT = 0x52;
        //constexpr uint8_t DELETE = 0x53;
    }
}