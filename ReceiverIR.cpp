/**
 * IR receiver (Version 0.0.4)
 *
 * Copyright (C) 2010 Shinichiro Nakamura (CuBeatSystems)
 * http://shinta.main.jp/
 */

// WARNING!
// This file have been changed by CTake1235 on 2023/08/24
// for mbedOS 6
// oridinaly text is comennted out 

#include "ReceiverIR.h"
#include "Callback.h"

#define LOCK()
#define UNLOCK()

#define InRange(x,y)   ((((y) * 0.7) < (x)) && ((x) < ((y) * 1.3)))

/**
 * Constructor.
 *
 * @param rxpin Pin for receive IR signal.
 */
ReceiverIR::ReceiverIR(PinName rxpin) : evt(rxpin) {
    init_state();
    // evt.fall(this, &ReceiverIR::isr_fall);
    // evt.rise(this, &ReceiverIR::isr_rise);
    evt.fall(Callback<void()>(this, &ReceiverIR::isr_fall));
    evt.rise(Callback<void()>(this, &ReceiverIR::isr_rise));
    evt.mode(PullUp);
    // ticker.attach_us(this, &ReceiverIR::isr_wdt, 10 * 1000);
    // ticker.attach_us(Callback<void ()>(this, &ReceiverIR::isr_wdt),10000);
    ticker.attach(Callback<void ()>(this, &ReceiverIR::isr_wdt),10ms);
}

/**
 * Destructor.
 */
ReceiverIR::~ReceiverIR() {
}

/**
 * Get state.
 *
 * @return Current state.
 */
ReceiverIR::State ReceiverIR::getState() {
    LOCK();
    State s = work.state;
    UNLOCK();
    return s;
}

/**
 * Get data.
 *
 * @param format Pointer to format.
 * @param buf Buffer of a data.
 * @param bitlength Bit length of the buffer.
 *
 * @return Data bit length.
 */
int ReceiverIR::getData(RemoteIR::Format *format, uint8_t *buf, int bitlength) {
    LOCK();

    if (bitlength < data.bitcount) {
        UNLOCK();
        return -1;
    }

    const int nbits = data.bitcount;
    const int nbytes = data.bitcount / 8 + (((data.bitcount % 8) != 0) ? 1 : 0);
    *format = data.format;
    for (int i = 0; i < nbytes; i++) {
        buf[i] = data.buffer[i];
    }

    init_state();

    UNLOCK();
    return nbits;
}

void ReceiverIR::init_state(void) {
    work.c1 = -1;
    work.c2 = -1;
    work.c3 = -1;
    work.d1 = -1;
    work.d2 = -1;
    work.state = Idle;
    data.format = RemoteIR::UNKNOWN;
    data.bitcount = 0;
    timer.stop();
    timer.reset();
    for (int i = 0; i < sizeof(data.buffer); i++) {
        data.buffer[i] = 0;
    }
}

void ReceiverIR::isr_wdt(void) {
    LOCK();
    static int cnt = 0;
    if ((Idle != work.state) || ((0 <= work.c1) || (0 <= work.c2) || (0 <= work.c3) || (0 <= work.d1) || (0 <= work.d2))) {
        cnt++;
        if (cnt > 50) {
#if 0
            printf("# WDT [c1=%d, c2=%d, c3=%d, d1=%d, d2=%d, state=%d, format=%d, bitcount=%d]\n",
                   work.c1,
                   work.c2,
                   work.c3,
                   work.d1,
                   work.d2,
                   work.state,
                   data.format,
                   data.bitcount);
#endif
            init_state();
            cnt = 0;
        }
    } else {
        cnt = 0;
    }
    UNLOCK();
}

void ReceiverIR::isr_fall(void) {
    LOCK();
    switch (work.state) {
        case Idle:
            if (work.c1 < 0) {
                timer.start();
                // work.c1 = timer.read_us();
                work.c1 = timer.elapsed_time().count();
            } else {
                // work.c3 = timer.read_us();
                work.c3 = timer.elapsed_time().count();
                int a = work.c2 - work.c1;
                int b = work.c3 - work.c2;
                if (InRange(a, RemoteIR::TUS_NEC * 16) && InRange(b, RemoteIR::TUS_NEC * 8)) {
                    /*
                     * NEC.
                     */
                    data.format = RemoteIR::NEC;
                    work.state = Receiving;
                    data.bitcount = 0;
                } else if (InRange(a, RemoteIR::TUS_NEC * 16) && InRange(b, RemoteIR::TUS_NEC * 4)) {
                    /*
                     * NEC Repeat.
                     */
                    data.format = RemoteIR::NEC_REPEAT;
                    work.state = Received;
                    data.bitcount = 0;
                    work.c1 = -1;
                    work.c2 = -1;
                    work.c3 = -1;
                    work.d1 = -1;
                    work.d2 = -1;
                } else if (InRange(a, RemoteIR::TUS_AEHA * 8) && InRange(b, RemoteIR::TUS_AEHA * 4)) {
                    /*
                     * AEHA.
                     */
                    data.format = RemoteIR::AEHA;
                    work.state = Receiving;
                    data.bitcount = 0;
                } else if (InRange(a, RemoteIR::TUS_AEHA * 8) && InRange(b, RemoteIR::TUS_AEHA * 8)) {
                    /*
                     * AEHA Repeat.
                     */
                    data.format = RemoteIR::AEHA_REPEAT;
                    work.state = Received;
                    data.bitcount = 0;
                    work.c1 = -1;
                    work.c2 = -1;
                    work.c3 = -1;
                    work.d1 = -1;
                    work.d2 = -1;
                } else {
                    init_state();
                }
            }
            break;
        case Receiving:
            if (RemoteIR::NEC == data.format) {
                // work.d2 = timer.read_us();
                work.d2 = timer.elapsed_time().count();
                int a = work.d2 - work.d1;
                if (InRange(a, RemoteIR::TUS_NEC * 3)) {
                    data.buffer[data.bitcount / 8] |= (1 << (data.bitcount % 8));
                } else if (InRange(a, RemoteIR::TUS_NEC * 1)) {
                    data.buffer[data.bitcount / 8] &= ~(1 << (data.bitcount % 8));
                }
                data.bitcount++;
#if 0
                /*
                 * Length of NEC is always 32 bits.
                 */
                if (32 <= data.bitcount) {
                    data.state = Received;
                    work.c1 = -1;
                    work.c2 = -1;
                    work.c3 = -1;
                    work.d1 = -1;
                    work.d2 = -1;
                }
#else
                /*
                 * Set timeout for tail detection automatically.
                 */
                timeout.detach();
                // timeout.attach_us(this, &ReceiverIR::isr_timeout, RemoteIR::TUS_NEC * 5);
                // timeout.attach_us(Callback<void()>(this,&ReceiverIR::isr_timeout),RemoteIR::TUS_NEC * 5);
                timeout.attach(Callback<void()>(this,&ReceiverIR::isr_timeout),RemoteIR::TUS_NEC * 5us);
#endif
            } else if (RemoteIR::AEHA == data.format) {
                // work.d2 = timer.read_us();
                work.d2 = timer.elapsed_time().count();
                int a = work.d2 - work.d1;
                if (InRange(a, RemoteIR::TUS_AEHA * 3)) {
                    data.buffer[data.bitcount / 8] |= (1 << (data.bitcount % 8));
                } else if (InRange(a, RemoteIR::TUS_AEHA * 1)) {
                    data.buffer[data.bitcount / 8] &= ~(1 << (data.bitcount % 8));
                }
                data.bitcount++;
#if 0
                /*
                 * Typical length of AEHA is 48 bits.
                 * Please check a specification of your remote controller if you find a problem.
                 */
                if (48 <= data.bitcount) {
                    data.state = Received;
                    work.c1 = -1;
                    work.c2 = -1;
                    work.c3 = -1;
                    work.d1 = -1;
                    work.d2 = -1;
                }
#else
                /*
                 * Set timeout for tail detection automatically.
                 */
                timeout.detach();
                // timeout.attach_us(this, &ReceiverIR::isr_timeout, RemoteIR::TUS_AEHA * 5);
                // timeout.attach_us(Callback<void()>(this,&ReceiverIR::isr_timeout), RemoteIR::TUS_AEHA * 5);
                timeout.attach(Callback<void()>(this,&ReceiverIR::isr_timeout), RemoteIR::TUS_AEHA * 5us);
#endif
            } else if (RemoteIR::SONY == data.format) {
                // work.d1 = timer.read_us();
                work.d1 = timer.elapsed_time().count();
            }
            break;
        case Received:
            break;
        default:
            break;
    }
    UNLOCK();
}

void ReceiverIR::isr_rise(void) {
    LOCK();
    switch (work.state) {
        case Idle:
            if (0 <= work.c1) {
                // work.c2 = timer.read_us();
                work.c2 = timer.elapsed_time().count();
                int a = work.c2 - work.c1;
                if (InRange(a, RemoteIR::TUS_SONY * 4)) {
                    data.format = RemoteIR::SONY;
                    work.state = Receiving;
                    data.bitcount = 0;
                } else {
                    static const int MINIMUM_LEADER_WIDTH = 150;
                    if (a < MINIMUM_LEADER_WIDTH) {
                        init_state();
                    }
                }
            } else {
                init_state();
            }
            break;
        case Receiving:
            if (RemoteIR::NEC == data.format) {
                // work.d1 = timer.read_us();
                work.d1 = timer.elapsed_time().count();
            } else if (RemoteIR::AEHA == data.format) {
                // work.d1 = timer.read_us();
                work.d1 = timer.elapsed_time().count();
            } else if (RemoteIR::SONY == data.format) {
                // work.d2 = timer.read_us();
                work.d2 = timer.elapsed_time().count();
                int a = work.d2 - work.d1;
                if (InRange(a, RemoteIR::TUS_SONY * 2)) {
                    data.buffer[data.bitcount / 8] |= (1 << (data.bitcount % 8));
                } else if (InRange(a, RemoteIR::TUS_SONY * 1)) {
                    data.buffer[data.bitcount / 8] &= ~(1 << (data.bitcount % 8));
                }
                data.bitcount++;
#if 0
                /*
                 * How do I know the correct length? (6bits, 12bits, 15bits, 20bits...)
                 * By a model only?
                 * Please check a specification of your remote controller if you find a problem.
                 */
                if (12 <= data.bitcount) {
                    data.state = Received;
                    work.c1 = -1;
                    work.c2 = -1;
                    work.c3 = -1;
                    work.d1 = -1;
                    work.d2 = -1;
                }
#else
                /*
                 * Set timeout for tail detection automatically.
                 */
                timeout.detach();
                // timeout.attach_us(this, &ReceiverIR::isr_timeout, RemoteIR::TUS_SONY * 4);
                
#endif
            }
            break;
        case Received:
            break;
        default:
            break;
    }
    UNLOCK();
}

void ReceiverIR::isr_timeout(void) {
    LOCK();
#if 0
    printf("# TIMEOUT [c1=%d, c2=%d, c3=%d, d1=%d, d2=%d, state=%d, format=%d, bitcount=%d]\n",
           work.c1,
           work.c2,
           work.c3,
           work.d1,
           work.d2,
           work.state,
           data.format,
           data.bitcount);
#endif
    if (work.state == Receiving) {
        work.state = Received;
        work.c1 = -1;
        work.c2 = -1;
        work.c3 = -1;
        work.d1 = -1;
        work.d2 = -1;
    }
    UNLOCK();
}
