/*
* (C) 2018,2024 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#ifndef BOTAN_CLI_TIMER_H_
#define BOTAN_CLI_TIMER_H_

#include <botan/types.h>
#include <chrono>
#include <string>

namespace Botan_CLI {

class Timer final {
   public:
      Timer(std::string_view name,
            std::string_view provider,
            std::string_view doing,
            uint64_t event_mult,
            size_t buf_size,
            double clock_cycle_ratio,
            uint64_t clock_speed);

      explicit Timer(std::string_view name) : Timer(name, "", "", 1, 0, 0.0, 0) {}

      Timer(std::string_view name, size_t buf_size) : Timer(name, "", "", buf_size, buf_size, 0.0, 0) {}

      void start();

      void stop();

      bool under(std::chrono::milliseconds msec) const {
         auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(msec);
         return (nano.count() >= 0) ? (value() < static_cast<size_t>(nano.count())) : true;
      }

      class Timer_Scope final {
         public:
            explicit Timer_Scope(Timer& timer) : m_timer(timer) { m_timer.start(); }

            ~Timer_Scope() {
               try {
                  m_timer.stop();
               } catch(...) {}
            }

            Timer_Scope(const Timer_Scope& other) = delete;
            Timer_Scope(Timer_Scope&& other) = delete;
            Timer_Scope& operator=(const Timer_Scope& other) = delete;
            Timer_Scope& operator=(Timer_Scope&& other) = delete;

         private:
            Timer& m_timer;
      };

      template <typename F>
      auto run(F f) -> decltype(f()) {
         Timer_Scope timer(*this);
         return f();
      }

      template <typename F>
      void run_until_elapsed(std::chrono::milliseconds msec, F f) {
         while(this->under(msec)) {
            run(f);
         }
      }

      uint64_t value() const { return m_time_used; }

      double seconds() const { return nanoseconds() / 1000000000.0; }

      double milliseconds() const { return nanoseconds() / 1000000.0; }

      double microseconds() const { return nanoseconds() / 1000.0; }

      double nanoseconds() const { return static_cast<double>(value()); }

      uint64_t cycles_consumed() const {
         if(m_clock_speed != 0) {
            return (m_clock_speed * value()) / 1000;
         }
         return m_cpu_cycles_used;
      }

      uint64_t events() const { return m_event_count * m_event_mult; }

      const std::string& get_name() const { return m_name; }

      const std::string& doing() const { return m_doing; }

      size_t buf_size() const { return m_buf_size; }

      double bytes_per_second() const { return events_per_second(); }

      double events_per_second() const {
         if(seconds() > 0.0 && events() > 0) {
            return static_cast<double>(events()) / seconds();
         } else {
            return 0.0;
         }
      }

      double seconds_per_event() const {
         if(seconds() > 0.0 && events() > 0) {
            return seconds() / static_cast<double>(events());
         } else {
            return 0.0;
         }
      }

      bool operator<(const Timer& other) const;

   private:
      static uint64_t timestamp_ns();
      static uint64_t cycle_counter();

      // const data
      std::string m_name, m_doing;
      size_t m_buf_size;
      uint64_t m_event_mult;
      double m_clock_cycle_ratio;
      uint64_t m_clock_speed;

      // set at runtime
      uint64_t m_event_count = 0;

      uint64_t m_time_used = 0;
      uint64_t m_timer_start = 0;

      uint64_t m_cpu_cycles_used = 0;
      uint64_t m_cpu_cycles_start = 0;
};

}  // namespace Botan_CLI

#endif
