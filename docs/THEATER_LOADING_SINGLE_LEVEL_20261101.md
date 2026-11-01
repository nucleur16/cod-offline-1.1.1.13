# Theatre loading image: single-level texture contract

The supplied Theatre loading zone embeds the complete original BO1 artwork as
one 1024x1024 BGRA level. Its image load definition used flags 1, which omit the
no-mipmap bit (2), although no smaller levels are embedded. The image header
already preserves the Factory template's single-level settings.

The builder now retains the original artwork's low two flags (3), requesting one
level with no picmip. It does not copy BO1-specific high bits or modify pixels,
dimensions, the material, state bits, technique, or other assets.

Evidence and limits:

- C1's stock inline `$black`, `$gray`, `$alpha`, and `$pixelcostcolorcode` images
  use one level with load flags 3.
- The local KisakBlack `r_image_load_common.cpp` reference's
  `Image_CountMipmaps` returns one for bit 2 and otherwise counts the chain;
  `Image_Setup` passes the same bit as the creation-level count. These are
  behavioral reference functions, not a claim of exact C1 function recovery.
- Building from the pinned Factory template and original BO1 IWD succeeds;
  the decoded original RGBA pixel hash remains
  `84ce9946fe443055ec1e3d825aca3f15cb377ba0d905fd684fe350076e59d295`.
- Independent baseline/candidate archive decoding finds exactly one changed
  byte in the uncompressed zone, at offset 1644: flags 1 becomes 3. All other
  bytes, including the complete pixel payload, remain identical.
- Candidate FF SHA256:
  `b99b081c73c1b8314befe59dd9c9cd241056e5672094bab76933bd4cb997c8a7`.

This repairs a concrete texture metadata inconsistency. A connected C1 loading
screen still needs visual acceptance before claiming the reported rectangular
black regions are resolved. The preview proves pixel preservation only. This
is a client asset change and must use the client/map-package release checks;
it is not a GSC-only release. No production files are changed by this builder.
