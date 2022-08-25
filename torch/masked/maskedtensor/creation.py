# Copyright (c) Meta Platforms, Inc. and affiliates

import torch

from .core import MaskedTensor, is_masked_tensor

__all__ = [
    "masked_tensor",
    "AsMaskedTensor",
    "as_masked_tensor"
]

def masked_tensor(data, mask, requires_grad=False):
    r""" A basic factory function to create a MaskedTensor

    Args:
        data: input data tensor
        mask: input mask tensor with dtype bool where True indicates "specified" and False indicates "unspecified"

    Examples::

        >>> data = torch.arange(6).reshape(2,3)
        >>> mask = torch.tensor([[True, False, False], [True, True, False]])
        >>> mt = masked_tensor(data, mask)
        >>> mt
        masked_tensor(
        [
            [0,       --,       --],
            [3, 4,       --]
        ]
        )
    """
    if is_masked_tensor(data):
        raise TypeError("data is already a MaskedTensor but must be a regular Tensor")
    if is_masked_tensor(mask):
        raise TypeError("mask is already a MaskedTensor but must be a regular Tensor")

    class Constructor(torch.autograd.Function):
        @staticmethod
        def forward(ctx, data, mask):
            return MaskedTensor(data.clone(), mask.clone(), requires_grad=requires_grad)

        @staticmethod
        def backward(ctx, grad_output):
            result = torch._masked._where(grad_output.get_mask(), grad_output.get_data(), 0)
            return result, None
            # return grad_output, None

    return Constructor.apply(data, mask)


# New function as_masked_tensor with autograd support to
# convert torch.Tensor into a MaskedTensor with some user-defined
# mask.
class AsMaskedTensor(torch.autograd.Function):
    @staticmethod
    def forward(ctx, data, mask):
        return MaskedTensor(data, mask)

    @staticmethod
    def backward(ctx, grad_output):
        return grad_output, None


def as_masked_tensor(data, mask):
    return AsMaskedTensor.apply(data, mask)
