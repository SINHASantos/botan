/*
* SecureQueue
* (C) 1999-2007 Jack Lloyd
*     2012 Markus Wanner
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#ifndef BOTAN_SECURE_QUEUE_H_
#define BOTAN_SECURE_QUEUE_H_

#include <botan/data_src.h>
#include <botan/filter.h>

namespace Botan {

/**
* A queue that knows how to zeroize itself
*/
class BOTAN_TEST_API SecureQueue final : public Fanout_Filter,
                                         public DataSource {
   public:
      std::string name() const override { return "Queue"; }

      void write(const uint8_t input[], size_t length) override;

      size_t read(uint8_t output[], size_t length) override;
      size_t peek(uint8_t output[], size_t length, size_t offset = 0) const override;
      size_t get_bytes_read() const override;

      bool end_of_data() const override;

      bool empty() const;

      bool check_available(size_t n) override { return n <= size(); }

      /**
      * @return number of bytes available in the queue
      */
      size_t size() const;

      bool attachable() override { return false; }

      /**
      * SecureQueue assignment
      * @param other the queue to copy
      */
      SecureQueue& operator=(const SecureQueue& other);

      SecureQueue& operator=(SecureQueue&& other) = delete;
      SecureQueue(SecureQueue&& other) = delete;

      /**
      * SecureQueue default constructor (creates empty queue)
      */
      SecureQueue();

      /**
      * SecureQueue copy constructor
      * @param other the queue to copy
      */
      SecureQueue(const SecureQueue& other);

      ~SecureQueue() override { destroy(); }

   private:
      void destroy();
      size_t m_bytes_read;
      class SecureQueueNode* m_head;
      class SecureQueueNode* m_tail;
};

}  // namespace Botan

#endif
