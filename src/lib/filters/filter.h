/*
* Filter
* (C) 1999-2007 Jack Lloyd
* (C) 2013 Joel Low
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#ifndef BOTAN_FILTER_H_
#define BOTAN_FILTER_H_

#include <botan/secmem.h>
#include <span>
#include <string>
#include <vector>

namespace Botan {

/**
* This class represents general abstract filter objects.
*/
class BOTAN_PUBLIC_API(2, 0) Filter {
   public:
      /**
      * @return descriptive name for this filter
      */
      virtual std::string name() const = 0;

      /**
      * Write a portion of a message to this filter.
      * @param input the input as a byte array
      * @param length the length of the byte array input
      */
      virtual void write(const uint8_t input[], size_t length) = 0;

      /**
      * Start a new message. Must be closed by end_msg() before another
      * message can be started.
      */
      virtual void start_msg() { /* default empty */
      }

      /**
      * Notify that the current message is finished; flush buffers and
      * do end-of-message processing (if any).
      */
      virtual void end_msg() { /* default empty */
      }

      /**
      * Check whether this filter is an attachable filter.
      * @return true if this filter is attachable, false otherwise
      */
      virtual bool attachable() { return true; }

      virtual ~Filter() = default;

      Filter(const Filter&) = delete;
      Filter(Filter&&) = delete;
      Filter& operator=(const Filter&) = delete;
      Filter& operator=(Filter&&) = delete;

   protected:
      /**
      * @param in some input for the filter
      * @param length the length of in
      */
      virtual void send(const uint8_t in[], size_t length);

      /**
      * @param in some input for the filter
      */
      void send(uint8_t in) { send(&in, 1); }

      /**
      * @param in some input for the filter
      */
      void send(std::span<const uint8_t> in) { send(in.data(), in.size()); }

      /**
      * @param in some input for the filter
      * @param length the number of bytes of in to send
      *
      * This previously took a std::vector, for which the length field (allowing
      * using just a prefix of the vector) somewhat made sense. It makes less
      * sense now that we are using a span here; you can just use `first` to get
      * a prefix.
      */
      void send(std::span<const uint8_t> in, size_t length);

      Filter();

   private:
      /**
      * Start a new message in *this and all following filters. Only for
      * internal use, not intended for use in client applications.
      */
      void new_msg();

      /**
      * End a new message in *this and all following filters. Only for
      * internal use, not intended for use in client applications.
      */
      void finish_msg();

      friend class Pipe;
      friend class Fanout_Filter;
      friend class Threaded_Fork;

      size_t total_ports() const;

      size_t current_port() const { return m_port_num; }

      /**
      * Set the active port
      * @param new_port the new value
      */
      void set_port(size_t new_port);

      size_t owns() const { return m_filter_owns; }

      /**
      * Attach another filter to this one
      * @param f filter to attach
      */
      void attach(Filter* f);

      /**
      * @param filters the filters to set
      * @param count number of items in filters
      */
      void set_next(Filter* filters[], size_t count);
      Filter* get_next() const;

      secure_vector<uint8_t> m_write_queue;
      std::vector<Filter*> m_next;  // not owned
      size_t m_port_num = 0;
      size_t m_filter_owns = 0;

      // true if filter belongs to a pipe --> prohibit filter sharing!
      bool m_owned = false;
};

/**
* This is the abstract Fanout_Filter base class.
**/
class BOTAN_PUBLIC_API(2, 0) Fanout_Filter : public Filter {
   protected:
      /**
      * Increment the number of filters past us that we own
      */
      void incr_owns() { ++m_filter_owns; }

      void set_port(size_t n) { Filter::set_port(n); }

      void set_next(Filter* f[], size_t n) { Filter::set_next(f, n); }

      void attach(Filter* f) { Filter::attach(f); }
};

/**
* The type of checking to be performed by decoders:
* NONE - no checks, IGNORE_WS - perform checks, but ignore
* whitespaces, FULL_CHECK - perform checks, also complain
* about white spaces.
*/
enum Decoder_Checking : uint8_t { NONE, IGNORE_WS, FULL_CHECK };

}  // namespace Botan

#endif
