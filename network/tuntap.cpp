// Copyright (C) by Josh Blum. See LICENSE.txt for licensing information.

#include <gras/block.hpp>
#include <gras/hier_block.hpp>
#include <gras/factory.hpp>
#include <boost/format.hpp>
#include <boost/make_shared.hpp>
#include <stdexcept>
#include <iostream>

static const long timeout_us = 100*1000; //100ms

#if defined(linux) || defined(__linux) || defined(__linux__)

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_tun.h>

int tun_alloc(char *dev, int flags = IFF_TAP | IFF_NO_PI) {

  struct ifreq ifr;
  int fd, err;
  const char *clonedev = "/dev/net/tun";

  /* Arguments taken by the function:
   *
   * char *dev: the name of an interface (or '\0'). MUST have enough
   *   space to hold the interface name if '\0' is passed
   * int flags: interface flags (eg, IFF_TUN etc.)
   */

   /* open the clone device */
   if( (fd = open(clonedev, O_RDWR)) < 0 ) {
     return fd;
   }

   /* preparation of the struct ifr, of type "struct ifreq" */
   memset(&ifr, 0, sizeof(ifr));

   ifr.ifr_flags = flags;   /* IFF_TUN or IFF_TAP, plus maybe IFF_NO_PI */

   if (*dev) {
     /* if a device name was specified, put it in the structure; otherwise,
      * the kernel will try to allocate the "next" device of the
      * specified type */
     strncpy(ifr.ifr_name, dev, IFNAMSIZ);
   }

   /* try to create the device */
   if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ) {
     close(fd);
     return err;
   }

  /* if the operation was successful, write back the name of the
   * interface to the variable "dev", so the caller can know
   * it. Note that the caller MUST reserve space in *dev (see calling
   * code below) */
  strcpy(dev, ifr.ifr_name);

  /* this is the special file descriptor that the caller will use to talk
   * with the virtual interface */
  return fd;
}

/***********************************************************************
 * Read datagram messages and write to fd
 **********************************************************************/
struct Datagram2Filedes : gras::Block
{
    Datagram2Filedes(const int fd):
        gras::Block("GREX Datagram2Filedes"),
        _fd(fd)
    {
        //NOP
    }

    void work(const InputItems &, const OutputItems &)
    {
        //read the input message, and check it
        const PMCC msg = this->pop_input_msg(0);
        if (not msg.is<gras::PacketMsg>()) return;

        //write the buffer into the file descriptor
        const gras::SBuffer &b = msg.as<gras::PacketMsg>().buff;
        const int result = write(_fd, b.get(), b.length);
        //std::cout << "wrote " << result << std::endl;
        if (result <= 0) std::cerr << "fildes -> write error " << result << std::endl;
    }

    const int _fd;
};

/***********************************************************************
 * Read from fd and post to downstream port
 **********************************************************************/
struct Filedes2Datagram : gras::Block
{
    Filedes2Datagram(const int fd):
        gras::Block("GREX Filedes2Datagram"),
        _fd(fd)
    {
        //setup the output for messages only
        this->output_config(0).reserve_items = 4096; //reasonably high mtu...
    }

    void work(const InputItems &, const OutputItems &)
    {
        //wait for a packet to become available
        if (not this->wait_ready()) return;

        //grab the output buffer to pass downstream as a msg
        gras::SBuffer b = this->get_output_buffer(0);

        //receive into the buffer
        const int result = read(_fd, b.get(), b.get_actual_length());
        if (result <= 0)
        {
            std::cerr << "fildes -> read error " << result << std::endl;
            return;
        }

        //create a message for this buffer
        const gras::PacketMsg msg(b);

        //post the output message downstream
        this->post_output_msg(0, msg);
    }

    bool wait_ready(void)
    {
        //setup timeval for timeout
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = timeout_us;

        //setup rset for timeout
        fd_set rset;
        FD_ZERO(&rset);
        FD_SET(_fd, &rset);

        //call select with timeout on receive socket
        return ::select(_fd+1, &rset, NULL, NULL, &tv) > 0;
    }

    const int _fd;
};

/***********************************************************************
 * Hier block that combines it all
 **********************************************************************/
struct TunTap : gras::HierBlock
{
    TunTap(const int fd, const std::string &dev_name):
        gras::HierBlock("GREX TunTap"),
        _fd(fd),
        _dev_name(dev_name)
    {
        //helpful user message
        std::cout << boost::format(
        "Allocated virtual ethernet interface: %s\n"
        "You must now use ifconfig to set its IP address. E.g.,\n"
        "  $ sudo ifconfig %s 192.168.200.1\n"
        "Be sure to use a different address in the same subnet for each machine.\n"
        ) % get_dev_name() % get_dev_name() << std::endl;

        //make the blocks
        _datagram_to_fildes = boost::make_shared<Datagram2Filedes>(_fd);
        _filedes_to_datagram = boost::make_shared<Filedes2Datagram>(_fd);

        //connect
        this->connect(*this, 0, _datagram_to_fildes, 0);
        this->connect(_filedes_to_datagram, 0, *this, 0);
    }

    ~TunTap(void)
    {
        close(_fd);
    }

    std::string get_dev_name(void)
    {
        return _dev_name;
    }

private:
    boost::shared_ptr<Filedes2Datagram> _filedes_to_datagram;
    boost::shared_ptr<Datagram2Filedes> _datagram_to_fildes;
    const int _fd;
    const std::string _dev_name;
};

/***********************************************************************
 * Factory function
 **********************************************************************/
gras::HierBlock *make_tuntap(const std::string &dev)
{
    //make the TunTap
    char dev_cstr[1024] = {};
    strncpy(dev_cstr, dev.c_str(), std::min(sizeof(dev_cstr), dev.size()));
    const int fd = tun_alloc(dev_cstr);
    if (fd <= 0)
    {
        throw std::runtime_error("TunTap make: tun_alloc failed");
    }
    return new TunTap(fd, dev_cstr);
}

#else //is not a linux

gras::HierBlock *make_tuntap(const std::string &)
{
    throw std::runtime_error("gr_make_TunTap: sorry, not implemented on this OS");
}

#endif //is a linux

GRAS_REGISTER_FACTORY("/grex/tuntap", make_tuntap)
