

#pragma once

#include <cstdint>
#include <cstdio>
#include <cmath>
#include <queue>
#include <functional>
#include <initializer_list>
#include <unordered_map>
#include <algorithm>
#include "SDL3/SDL.h"




#define KBD_STATUS_OBF      0x01  
#define KBD_STATUS_IBF      0x02  
#define KBD_STATUS_SYS      0x04  
#define KBD_STATUS_CD       0x08  
#define KBD_STATUS_KEYLOCK  0x10  
#define KBD_STATUS_AUXOBF   0x20  
#define KBD_STATUS_TIMEOUT  0x40  
#define KBD_STATUS_PARITY   0x80  




#define KBD_CMD_READ_CCB        0x20
#define KBD_CMD_WRITE_CCB       0x60
#define KBD_CMD_DISABLE_AUX     0xA7
#define KBD_CMD_ENABLE_AUX      0xA8
#define KBD_CMD_TEST_AUX        0xA9
#define KBD_CMD_SELF_TEST       0xAA
#define KBD_CMD_TEST_KBD        0xAB
#define KBD_CMD_DISABLE_KBD     0xAD
#define KBD_CMD_ENABLE_KBD      0xAE
#define KBD_CMD_READ_INPUT      0xC0
#define KBD_CMD_READ_OUTPUT     0xD0
#define KBD_CMD_WRITE_OUTPUT    0xD1
#define KBD_CMD_WRITE_AUX       0xD4   
#define KBD_CMD_PULSE_OUTPUT    0xF0
#define KBD_CMD_CPU_RESET       0xFE   




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


#define KBD_RESP_ACK        0xFA
#define KBD_RESP_RESEND     0xFE
#define KBD_RESP_BAT_OK     0xAA


#define KBD_CCB_INT         0x01  
#define KBD_CCB_AUX_INT     0x02  
#define KBD_CCB_SYS         0x04  
#define KBD_CCB_DISABLE_KBD 0x10  
#define KBD_CCB_DISABLE_AUX 0x20  
#define KBD_CCB_TRANSLATE   0x40  




#define MOUSE_CMD_RESET          0xFF
#define MOUSE_CMD_RESEND         0xFE
#define MOUSE_CMD_SET_DEFAULT    0xF6
#define MOUSE_CMD_DISABLE        0xF5
#define MOUSE_CMD_ENABLE         0xF4
#define MOUSE_CMD_SET_SAMPLE     0xF3
#define MOUSE_CMD_GET_ID         0xF2
#define MOUSE_CMD_SET_REMOTE     0xF0
#define MOUSE_CMD_SET_WRAP       0xEE
#define MOUSE_CMD_RESET_WRAP     0xEC
#define MOUSE_CMD_READ_DATA      0xEB
#define MOUSE_CMD_SET_STREAM     0xEA
#define MOUSE_CMD_STATUS         0xE9
#define MOUSE_CMD_SET_RES        0xE8
#define MOUSE_CMD_SET_SCALE21    0xE7
#define MOUSE_CMD_SET_SCALE11    0xE6




using RaiseIRQ_fn = std::function<void(int irq)>;
using RebootFn = std::function<void()>;




struct OutputByte {
    uint8_t val;
    bool    from_mouse;
};




class ByteQueue {
    std::queue<uint8_t> q;
    const std::size_t   cap;
public:
    explicit ByteQueue(std::size_t capacity = 1024) : cap(capacity) {}

    void push(uint8_t v) {
        if (q.size() < cap) q.push(v);
    }
    uint8_t shift() {
        if (q.empty()) return 0;
        uint8_t v = q.front(); q.pop(); return v;
    }
    void clear() { while (!q.empty()) q.pop(); }
    std::size_t length() const { return q.size(); }
};




class PS2Mouse {
public:
    bool left_btn = false;
    bool middle_btn = false;
    bool right_btn = false;
    bool     enable_stream = false;   
    bool     use_mouse = false;   
    bool     have_mouse = true;    

    int16_t  delta_x = 0;       
    int16_t  delta_y = 0;       
    uint8_t  mouse_clicks = 0;       
    int8_t   wheel_movement = 0;       

    uint8_t  sample_rate = 100;     
    uint8_t  resolution = 4;       
    bool     scaling2 = false;   
    uint8_t  mouse_id = 0x00;    
    int      mouse_detect_state = 0;    
    bool     mouse_reset_workaround = false; 

    
    bool     expecting_sample = false;
    bool     expecting_res = false;

    
    std::queue<OutputByte> output_buf;

    void push(uint8_t val) {
        output_buf.push({ val, true });
    }

    
    
    
    void reset() {
        enable_stream = false;
        sample_rate = 100;
        scaling2 = false;
        resolution = 4;
        delta_x = delta_y = 0;
        mouse_clicks = 0;
        wheel_movement = 0;
        mouse_detect_state = 0;

        if (!mouse_reset_workaround) {
            mouse_id = 0x00;
        }

        
        push(0xAA);
        push(mouse_id);
    }

    
    
    
    void set_sample_rate(uint8_t rate) {
        sample_rate = rate;
        if (!sample_rate) {
            printf("[MOUSE] invalid sample rate, reset to 100\n");
            sample_rate = 100;
        }

        
        switch (mouse_detect_state) {
        case -1:
            if (rate == 60) {
                
                mouse_reset_workaround = true;
                mouse_detect_state = 0;
            }
            else {
                mouse_reset_workaround = false;
                mouse_detect_state = (rate == 200) ? 1 : 0;
            }
            break;
        case 0:
            if (rate == 200) mouse_detect_state = 1;
            break;
        case 1:
            if (rate == 100) mouse_detect_state = 2;
            else if (rate == 200) mouse_detect_state = 3;
            else                  mouse_detect_state = 0;
            break;
        case 2:
            
            if (rate == 80) mouse_id = 0x03;
            mouse_detect_state = -1;
            break;
        case 3:
            
            if (rate == 80) mouse_id = 0x04;
            mouse_detect_state = -1;
            break;
        }

        printf("[MOUSE] sample rate: %u, mouse_id: 0x%02X\n", rate, mouse_id);
    }

    
    
    
    void send_packet(int16_t dx = 0, int16_t dy = 0) {
        uint8_t info_byte =
            static_cast<uint8_t>(
                ((dy < 0) ? (1 << 5) : 0) |
                ((dx < 0) ? (1 << 4) : 0) |
                (1 << 3) |          
                (mouse_clicks & 0x07)
                );

        push(info_byte);
        push(static_cast<uint8_t>(dx));
        push(static_cast<uint8_t>(dy));

        if (mouse_id == 0x04) {
            
            push(static_cast<uint8_t>(wheel_movement & 0x0F));
            wheel_movement = 0;
        }
        else if (mouse_id == 0x03) {
            
            push(static_cast<uint8_t>(wheel_movement & 0xFF));
            wheel_movement = 0;
        }
    }

    
    
    
    void send_delta(float fdx, float fdy) {
        if (!have_mouse || !use_mouse) return;

        delta_x += static_cast<int16_t>(fdx);
        delta_y += static_cast<int16_t>(fdy);

        if (enable_stream) {
            int16_t cx = static_cast<int16_t>(delta_x);
            int16_t cy = static_cast<int16_t>(delta_y);
            if (cx || cy) {
                delta_x -= cx;
                delta_y -= cy;
                send_packet(cx, cy);
            }
        }
    }

    
    
    
    void send_click(bool left, bool middle, bool right) {
        if (!have_mouse || !use_mouse) return;
        mouse_clicks = static_cast<uint8_t>(
            (left ? 0x01 : 0) |
            (right ? 0x02 : 0) |
            (middle ? 0x04 : 0)
            );
        if (enable_stream) send_packet(0, 0);
    }

    
    
    
    void send_wheel(int wx, int wy) {
        if (!have_mouse || !use_mouse) return;
        wheel_movement -= static_cast<int8_t>(wx);
        wheel_movement -= static_cast<int8_t>(wy * 2);
        
        wheel_movement = static_cast<int8_t>(
            min(7, max(-8, static_cast<int>(wheel_movement)))
            );
        send_packet(0, 0);
    }

    
    
    
    
    bool handle_command(uint8_t cmd) {
        
        if (expecting_sample) {
            expecting_sample = false;
            push(0xFA);
            set_sample_rate(cmd);
            return true;
        }
        if (expecting_res) {
            expecting_res = false;
            if (cmd > 3) {
                resolution = 4;
                printf("[MOUSE] invalid resolution, resetting to 4\n");
            }
            else {
                resolution = 1 << cmd;
                printf("[MOUSE] resolution: %u\n", resolution);
            }
            push(0xFA);
            return true;
        }

        
        while (!output_buf.empty()) output_buf.pop();

        switch (cmd) {
        case MOUSE_CMD_SET_SCALE11:         
            push(0xFA);
            scaling2 = false;
            printf("[MOUSE] Scaling 1:1\n");
            break;

        case MOUSE_CMD_SET_SCALE21:         
            push(0xFA);
            scaling2 = true;
            printf("[MOUSE] Scaling 2:1\n");
            break;

        case MOUSE_CMD_SET_RES:             
            push(0xFA);
            expecting_res = true;
            break;

        case MOUSE_CMD_STATUS: {            
            push(0xFA);
            send_packet(0, 0);
            break;
        }

        case MOUSE_CMD_READ_DATA:           
            printf("[MOUSE] request single packet\n");
            push(0xFA);
            send_packet(0, 0);
            break;

        case MOUSE_CMD_GET_ID:              
            printf("[MOUSE] required id: 0x%02X\n", mouse_id);
            push(0xFA);
            push(mouse_id);
            mouse_clicks = delta_x = delta_y = 0;
            break;

        case MOUSE_CMD_SET_SAMPLE:          
            push(0xFA);
            expecting_sample = true;
            break;

        case MOUSE_CMD_ENABLE:              
            push(0xFA);
            enable_stream = true;
            use_mouse = true;
            mouse_clicks = delta_x = delta_y = 0;
            printf("[MOUSE] streaming enabled\n");
            break;

        case MOUSE_CMD_DISABLE:             
            push(0xFA);
            enable_stream = false;
            printf("[MOUSE] streaming disabled\n");
            break;

        case MOUSE_CMD_SET_DEFAULT:         
            push(0xFA);
            enable_stream = false;
            sample_rate = 100;
            scaling2 = false;
            resolution = 4;
            break;

        case MOUSE_CMD_RESET:               
            push(0xFA);
            printf("[MOUSE] Mouse reset\n");
            use_mouse = true;
            enable_stream = false;
            sample_rate = 100;
            scaling2 = false;
            resolution = 4;
            reset();        
            mouse_clicks = delta_x = delta_y = 0;
            break;

        case MOUSE_CMD_SET_STREAM:          
            push(0xFA);
            enable_stream = true;
            break;

        case MOUSE_CMD_SET_WRAP:            
        case MOUSE_CMD_RESET_WRAP:          
        case MOUSE_CMD_SET_REMOTE:          
            push(0xFA);
            break;

        default:
            printf("[MOUSE] Unimplemented mouse command: 0x%02X\n", cmd);
            push(0xFA);
            break;
        }
        return true;
    }
};




class PS2Keyboard {
public:
    std::queue<OutputByte> output_buf; 
    std::queue<OutputByte> input_buf;

    uint8_t status_reg;
    uint8_t ccb;            
    uint8_t last_cmd;
    bool    expecting_data; 
    bool    kbd_disabled;
    bool    aux_disabled;

    uint8_t kbd_leds;
    uint8_t scan_mode;
    bool    expecting_scanmode;
    bool    expecting_rate;
    bool    expecting_led;

    PS2Mouse mouse;

    bool     next_is_mouse; 

    
    bool read_output_register;       
    bool read_command_register;      
    bool read_controller_output_port;
    uint8_t controller_output_port;  

    RaiseIRQ_fn raise_irq;
    RebootFn    do_reboot;

    PS2Keyboard(RaiseIRQ_fn irq_fn, RebootFn reboot_fn = nullptr)
        : status_reg(KBD_STATUS_SYS | KBD_STATUS_KEYLOCK)
        , ccb(KBD_CCB_INT | KBD_CCB_AUX_INT | KBD_CCB_TRANSLATE | KBD_CCB_SYS)
        
        , last_cmd(0)
        , expecting_data(false)
        , kbd_disabled(false)
        , aux_disabled(false)
        , kbd_leds(0)
        , scan_mode(2)
        , expecting_scanmode(false)
        , expecting_rate(false)
        , expecting_led(false)
        , next_is_mouse(false)
        , read_output_register(false)
        , read_command_register(false)
        , read_controller_output_port(false)
        , controller_output_port(0)
        , raise_irq(irq_fn)
        , do_reboot(reboot_fn)
    {}

    
    
    
    void push_output(uint8_t val, bool from_mouse = false) {
        output_buf.push({ val, from_mouse });
        status_reg |= KBD_STATUS_OBF;
        // Only update AUXOBF based on the FRONT of the queue (what guest reads next)
        if (output_buf.front().from_mouse)
            status_reg |= KBD_STATUS_AUXOBF;
        else
            status_reg &= ~KBD_STATUS_AUXOBF;
    }

    
    
    
    void flush_mouse_output() {
        while (!mouse.output_buf.empty()) {
            uint8_t v = mouse.output_buf.front().val;
            mouse.output_buf.pop();
            push_output(v, true);
        }
        trigger_mouse_irq();
    }

    
    
    

    
    void trigger_mouse_irq() {
        if (ccb & KBD_CCB_AUX_INT) {
            raise_irq(12);
        }
    }

    
    void trigger_irq() {
        if (ccb & KBD_CCB_INT) {
            raise_irq(1);
        }
    }

    
    void raise_irq_smart() {
        if (!output_buf.empty()) {
            if (!output_buf.front().from_mouse)
                trigger_irq();
            else
                trigger_mouse_irq();
        }
    }

    
    
    
    uint8_t read_data() {
        if (output_buf.empty()) {
            
            return 0x00;
        }

        OutputByte ob = output_buf.front();
        output_buf.pop();

        if (output_buf.empty()) {
            status_reg &= ~KBD_STATUS_OBF;
            status_reg &= ~KBD_STATUS_AUXOBF;
        }
        else {
            
            if (output_buf.front().from_mouse)
                status_reg |= KBD_STATUS_AUXOBF;
            else
                status_reg &= ~KBD_STATUS_AUXOBF;

            
            raise_irq_smart();
        }

        
        return ob.val;
    }

    
    
    
    uint8_t read_status() {
        
        uint8_t s = 0x10;
        if (!output_buf.empty())                    s |= KBD_STATUS_OBF;
        if (!output_buf.empty() &&
            output_buf.front().from_mouse)          s |= KBD_STATUS_AUXOBF;
        
        return s;
    }

    
    
    
    void write_command(uint8_t cmd) {
        

        last_cmd = cmd;
        expecting_data = false;
        next_is_mouse = false;

        switch (cmd) {
        case KBD_CMD_READ_CCB:          
            while (!output_buf.empty()) output_buf.pop();
            mouse.output_buf = {};      
            push_output(ccb);
            trigger_irq();
            break;

        case KBD_CMD_WRITE_CCB:         
            read_command_register = true;
            break;

        case KBD_CMD_WRITE_OUTPUT:      
            read_controller_output_port = true;
            break;

        case KBD_CMD_READ_OUTPUT:       
            read_output_register = true;
            break;

        case KBD_CMD_WRITE_AUX:         
            next_is_mouse = true;
            break;

        case KBD_CMD_DISABLE_AUX:       
            
            ccb |= KBD_CCB_DISABLE_AUX;
            aux_disabled = true;
            break;

        case KBD_CMD_ENABLE_AUX:        
            
            ccb &= ~KBD_CCB_DISABLE_AUX;
            aux_disabled = false;
            break;

        case KBD_CMD_TEST_AUX:          
            while (!output_buf.empty()) output_buf.pop();
            mouse.output_buf = {};
            push_output(0x00);
            trigger_irq();
            break;

        case KBD_CMD_SELF_TEST:         
            while (!output_buf.empty()) output_buf.pop();
            mouse.output_buf = {};
            push_output(0x55);
            status_reg |= KBD_STATUS_SYS;
            ccb |= KBD_CCB_SYS;
            trigger_irq();
            break;

        case KBD_CMD_TEST_KBD:          
            while (!output_buf.empty()) output_buf.pop();
            mouse.output_buf = {};
            push_output(0x00);
            trigger_irq();
            break;

        case KBD_CMD_DISABLE_KBD:       
            
            ccb |= KBD_CCB_DISABLE_KBD;
            kbd_disabled = true;
            break;

        case KBD_CMD_ENABLE_KBD:        
            
            ccb &= ~KBD_CCB_DISABLE_KBD;
            kbd_disabled = false;
            break;

        case KBD_CMD_CPU_RESET:         
            printf("[PS2] CPU reboot via PS2\n");
            if (do_reboot) do_reboot();
            break;

        default:
            printf("[PS2] port 64: Unimplemented command: 0x%02X\n", cmd);
            break;
        }
    }
    void write_data(uint8_t val) {
        printf("[PS2] port 60 write: 0x%02X\n", val);

        status_reg &= ~KBD_STATUS_IBF;

        
        if (read_command_register) {
            read_command_register = false;
            ccb = val;
            kbd_disabled = (ccb & KBD_CCB_DISABLE_KBD) != 0;
            aux_disabled = (ccb & KBD_CCB_DISABLE_AUX) != 0;
            printf("[PS2] Keyboard command register = 0x%02X\n", ccb);
            return;
        }

        
        if (read_output_register) {
            read_output_register = false;
            while (!output_buf.empty()) output_buf.pop();
            push_output(val, true);  
            trigger_mouse_irq();
            return;
        }

        
        if (mouse.expecting_sample) {
            
            mouse.handle_command(val);
            flush_mouse_output();
            return;
        }

        
        if (mouse.expecting_res) {
            mouse.handle_command(val);
            flush_mouse_output();
            return;
        }

        
        if (expecting_led) {
            expecting_led = false;
            kbd_leds = val & 0x07;
            push_output(KBD_RESP_ACK);
            trigger_irq();
            printf("[PS2] LEDs: scroll=%d num=%d caps=%d\n",
                kbd_leds & 1, (kbd_leds >> 1) & 1, (kbd_leds >> 2) & 1);
            return;
        }

        
        if (expecting_scanmode) {
            expecting_scanmode = false;
            push_output(KBD_RESP_ACK);
            if (val == 0) {
                push_output(1);  
            }
            else {
                scan_mode = val;
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

        
        if (next_is_mouse) {
            next_is_mouse = false;
            printf("[PS2] Port 60 data register write (mouse): 0x%02X\n", val);

            if (!mouse.have_mouse) return;

            
            while (!output_buf.empty()) output_buf.pop();
            mouse.output_buf = {};

            mouse.handle_command(val);
            flush_mouse_output();
            return;
        }

        
        if (read_controller_output_port) {
            read_controller_output_port = false;
            controller_output_port = val;
            
            return;
        }

        
        printf("[PS2] Port 60 data register write (kbd): 0x%02X\n", val);
        handle_kbd_command(val);
    }

    
    
    
    void handle_kbd_command(uint8_t val) {
        
        while (!output_buf.empty()) output_buf.pop();
        mouse.output_buf = {};
        push_output(KBD_RESP_ACK);

        switch (val) {
        case KBD_DEV_CMD_SET_LEDS:          
            expecting_led = true;
            break;

        case KBD_DEV_CMD_SCAN_MODE:         
            expecting_scanmode = true;
            break;

        case KBD_DEV_CMD_GET_ID:            
            push_output(0xAB);
            push_output(0x83);
            break;

        case KBD_DEV_CMD_SET_RATE:          
            expecting_rate = true;
            break;

        case KBD_DEV_CMD_ENABLE:            
            printf("[PS2] kbd enable scanning\n");
            kbd_disabled = false;
            break;

        case KBD_DEV_CMD_DISABLE:           
            printf("[PS2] kbd disable scanning\n");
            kbd_disabled = true;
            break;

        case KBD_DEV_CMD_SET_DEFAULT:       
            
            break;

        case KBD_DEV_CMD_RESET:             
            
            while (!output_buf.empty()) output_buf.pop();
            push_output(KBD_RESP_ACK);
            push_output(KBD_RESP_BAT_OK);
            push_output(0x00);
            break;

        case KBD_DEV_CMD_ECHO:              
            while (!output_buf.empty()) output_buf.pop();
            push_output(0xEE);
            break;

        case KBD_DEV_CMD_RESEND:            
            
            break;

        default:
            printf("[PS2] Unimplemented keyboard command: 0x%02X\n", val);
            break;
        }

        trigger_irq();
    }

    
    
    
    void send_scancode(uint8_t code) {
        if (kbd_disabled) return;
        push_output(code, false);
        // Only raise kbd IRQ if the byte we just pushed is at the front
        // (i.e. no mouse data was already waiting)
        if (!output_buf.front().from_mouse)
            trigger_irq();
        else
            trigger_mouse_irq();  // let mouse drain first
    }

    void send_scancodes(std::initializer_list<uint8_t> codes) {
        if (kbd_disabled) return;
        for (uint8_t c : codes) push_output(c, false);
        trigger_irq();
    }
};




namespace Scancode1 {
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
    constexpr uint8_t UP = 0x48;
    constexpr uint8_t DOWN = 0x50;
    constexpr uint8_t LEFT = 0x4B;
    constexpr uint8_t RIGHT = 0x4D;

    
    constexpr uint8_t BREAK(uint8_t make) { return make | 0x80; }

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
    }
}




struct KeyEntry {
    bool    extended;
    uint8_t scancode;
};

static const std::unordered_map<SDL_Keycode, KeyEntry> keymap = {
    
    { SDLK_A, { false, Scancode1::A } }, { SDLK_B, { false, Scancode1::B } },
    { SDLK_C, { false, Scancode1::C } }, { SDLK_D, { false, Scancode1::D } },
    { SDLK_E, { false, Scancode1::E } }, { SDLK_F, { false, Scancode1::F } },
    { SDLK_G, { false, Scancode1::G } }, { SDLK_H, { false, Scancode1::H } },
    { SDLK_I, { false, Scancode1::I } }, { SDLK_J, { false, Scancode1::J } },
    { SDLK_K, { false, Scancode1::K } }, { SDLK_L, { false, Scancode1::L } },
    { SDLK_M, { false, Scancode1::M } }, { SDLK_N, { false, Scancode1::N } },
    { SDLK_O, { false, Scancode1::O } }, { SDLK_P, { false, Scancode1::P } },
    { SDLK_Q, { false, Scancode1::Q } }, { SDLK_R, { false, Scancode1::R } },
    { SDLK_S, { false, Scancode1::S } }, { SDLK_T, { false, Scancode1::T } },
    { SDLK_U, { false, Scancode1::U } }, { SDLK_V, { false, Scancode1::V } },
    { SDLK_W, { false, Scancode1::W } }, { SDLK_X, { false, Scancode1::X } },
    { SDLK_Y, { false, Scancode1::Y } }, { SDLK_Z, { false, Scancode1::Z } },

    
    { SDLK_1, { false, Scancode1::KEY_1 } }, { SDLK_2, { false, Scancode1::KEY_2 } },
    { SDLK_3, { false, Scancode1::KEY_3 } }, { SDLK_4, { false, Scancode1::KEY_4 } },
    { SDLK_5, { false, Scancode1::KEY_5 } }, { SDLK_6, { false, Scancode1::KEY_6 } },
    { SDLK_7, { false, Scancode1::KEY_7 } }, { SDLK_8, { false, Scancode1::KEY_8 } },
    { SDLK_9, { false, Scancode1::KEY_9 } }, { SDLK_0, { false, Scancode1::KEY_0 } },

    
    { SDLK_F1,  { false, Scancode1::F1  } }, { SDLK_F2,  { false, Scancode1::F2  } },
    { SDLK_F3,  { false, Scancode1::F3  } }, { SDLK_F4,  { false, Scancode1::F4  } },
    { SDLK_F5,  { false, Scancode1::F5  } }, { SDLK_F6,  { false, Scancode1::F6  } },
    { SDLK_F7,  { false, Scancode1::F7  } }, { SDLK_F8,  { false, Scancode1::F8  } },
    { SDLK_F9,  { false, Scancode1::F9  } }, { SDLK_F10, { false, Scancode1::F10 } },
    { SDLK_F11, { false, Scancode1::F11 } }, { SDLK_F12, { false, Scancode1::F12 } },

    
    { SDLK_UP,    { true, Scancode1::Extended::UP    } },
    { SDLK_DOWN,  { true, Scancode1::Extended::DOWN  } },
    { SDLK_LEFT,  { true, Scancode1::Extended::LEFT  } },
    { SDLK_RIGHT, { true, Scancode1::Extended::RIGHT } },

    
    { SDLK_INSERT,   { true, Scancode1::Extended::INSERT } },
    { SDLK_HOME,     { true, Scancode1::Extended::HOME   } },
    { SDLK_END,      { true, Scancode1::Extended::END    } },
    { SDLK_PAGEUP,   { true, Scancode1::Extended::PGUP   } },
    { SDLK_PAGEDOWN, { true, Scancode1::Extended::PGDN   } },

    
    { SDLK_LSHIFT,   { false, Scancode1::LSHIFT               } },
    { SDLK_RSHIFT,   { false, Scancode1::RSHIFT               } },
    { SDLK_LCTRL,    { false, Scancode1::LCTRL                } },
    { SDLK_RCTRL,    { true,  Scancode1::Extended::RCTRL      } },
    { SDLK_LALT,     { false, Scancode1::LALT                 } },
    { SDLK_RALT,     { true,  Scancode1::Extended::RALT       } },
    { SDLK_CAPSLOCK, { false, Scancode1::CAPSLOCK             } },
    { SDLK_TAB,      { false, Scancode1::TAB                  } },

    
    { SDLK_RETURN,    { false, Scancode1::ENTER     } },
    { SDLK_ESCAPE,    { false, Scancode1::ESC       } },
    { SDLK_SPACE,     { false, Scancode1::SPACE     } },
    { SDLK_BACKSPACE, { false, Scancode1::BACKSPACE } },
    { SDLK_MINUS,     { false, Scancode1::MINUS     } },
    { SDLK_EQUALS,    { false, Scancode1::EQUALS    } },
};