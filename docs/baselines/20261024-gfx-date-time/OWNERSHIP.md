# Ownership and proof boundaries

This bundle contains source fixtures, reference data and nonsecret explanations.
It contains no game image, DLL, object, packet capture, absolute private path,
credential, clock manipulation, game process launch or UI automation.

The production implementation owns no Date object or output allocation. Both
belong to the existing synchronous AS member call. It calls the immutable native
member first, reads its clipped epoch, and writes only the existing four-byte
local offset field. It retains no receiver, output or OS structure pointer.

The only installed modification is the verified initializer's immediate target.
Original method bytes, adjustment initializer, epoch field and clipped return
remain under the original engine's ownership. A repeated/late installer sees
the nonzero initialized pointer or changed initializer and refuses installation.

Use separate explicit scratch output and owned image inputs. Private binaries
and generated bridge/test executables must remain outside Git. The checked-in
policy regression needs no private input. Tests and explanatory proof are not
authorization to restart or publish a client, lobby or match.
