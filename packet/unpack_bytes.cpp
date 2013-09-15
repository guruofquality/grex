// Copyright (C) by Josh Blum. See LICENSE.txt for licensing information.

#include <gras/block.hpp>
#include <gras/factory.hpp>

struct UnpackBytes : gras::Block
{
    UnpackBytes(void):
        gras::Block("GREX UnpackBytes")
    {
        return;
    }

    void work(const InputItems &ins, const OutputItems &outs)
    {
        const unsigned char *in = ins[0].cast<const unsigned char *>();
        unsigned char *out = outs[0].cast<unsigned char *>();

        const size_t n = std::min(ins[0].size(), outs[0].size()/8);
        if (n == 0) this->mark_output_fail(0);

        size_t o = 0;
        for (size_t i = 0; i < n; i++)
        {
            out[o++] = (in[i] >> 7) & 0x1;
            out[o++] = (in[i] >> 6) & 0x1;
            out[o++] = (in[i] >> 5) & 0x1;
            out[o++] = (in[i] >> 4) & 0x1;
            out[o++] = (in[i] >> 3) & 0x1;
            out[o++] = (in[i] >> 2) & 0x1;
            out[o++] = (in[i] >> 1) & 0x1;
            out[o++] = (in[i] >> 0) & 0x1;
        }

        this->consume(n);
        this->produce(n*8);
    }
};

GRAS_REGISTER_FACTORY0("/grex/unpack_bytes", UnpackBytes)
