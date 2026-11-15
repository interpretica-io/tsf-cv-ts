# tsf-cv-ts

A Test Environment suite that exercises
[tsf-cv](https://github.com/interpretica-io/tsf-cv) (`tapi_cv`) against
the agent it runs on — computer vision over OpenCV, with no key and no
network (OpenCV is a library linked into the agent's RPC server). Every
test draws its own deterministic images on the agent and asserts tsf-cv
reads them back correctly.

| Test | What it checks |
|---|---|
| `info` | the synthetic scene is exactly 320×240, 3-channel, 8-bit PNG |
| `compare` | two copies of the scene are identical (zero changed pixels, SSIM 1); the diff variant's one extra rectangle is found and boxed where it was drawn |
| `match` | the red block, cropped out as a template, is found at the pixel it was cropped from; a noise template that is not in the scene is not found |
| `region` | the green "indicator" rectangle is ~100% inside a green HSV range and the red block is ~0%, with a red-dominant mean colour |
| `ocr` | the rendered string `HELLO CV 2026` is recovered by OCR (skipped when the agent has no tesseract) |
| `decode` | a QR encoding `tsf-cv:ok` is decoded back (skipped when the agent's OpenCV cannot encode the fixture) |

OCR and barcode decoding are **optional backends**; a test skips
(`tapi_cv_supports()`) when the agent does not have the one it needs,
rather than failing. There is no mock and no network: OpenCV runs in the
agent's RPC server, called through its own C++ library API, so every
image is analysed locally and deterministically. Numeric results are
emitted as MI measurement artifacts, so the HTML log and Bublik draw
them as graphs (changed pixels over tolerance, match score by method
family, green share by region, OCR confidence per word).

## Fixtures

The images are not committed. Each test writes a small cv2/numpy
generator onto the agent and runs it (the same idea as tsf-ai-ts's mock
provider), so the scene — its geometry, colours, text and QR payload —
is known exactly, and the generator's drawing is kept in step with the
`TSAPI_CV_*` macros the tests assert against. A host whose python3 has
no `cv2` skips the suite cleanly.

## Running it

Needs Docker and `test-environment` as a sibling directory:

```bash
./scripts/run.sh docker guess --cfg=localhost
```

On the `localhost` configuration the agent is the build container, so
that container must carry **OpenCV with its development headers**
(`libopencv-dev`) and a **C++ compiler**; `libtesseract-dev` with an
English language pack enables the OCR test, and `python3-opencv` /
`python3-numpy` draw the fixtures. The provided
`scripts/docker/build/Dockerfile` installs all of these. Name a host in
`conf/rcf.conf` and the same tests run against it.

`conf/external.yml` names the tsf-cv repository and the ref to follow;
`conf/builder.conf.lock` pins the commit actually built (empty until the
first build, since tsf-cv is not published yet). The Builder clones
tsf-cv itself, so the checkout beside this suite is not what a run
compiles.

## Tests

| Test | What it checks |
|---|---|
| `info` | the scene is exactly 320×240, 3-channel, 8-bit PNG |
| `formats` | container and depth across JPEG, BMP, TIFF, an 8-bit 1-channel PNG and a 16-bit 3-channel PNG |
| `compare` | identical copies (0 changed, SSIM 1); the diff variant boxed where it changed; the heatmap read back as a real scene-sized image |
| `match` | the red block found at its pixel, an absent template not found, and the same template found at both places it appears |
| `region` | the green indicator ~100 % inside a green HSV range, the red block ~0 % |
| `ocr` | the rendered caption recovered whole and from a sub-rectangle (skips without tesseract) |
| `decode` | one QR symbol, and two QR symbols in one image, decoded back |
| `barcode` | OpenCV's own reference EAN-13 photos decode to their known values (the control); the synthetic one is a non-fatal diagnostic |
| `screenshot` | end-to-end on one realistic app screenshot: size, button match, green indicator, OCR'd status line, decoded QR |
| `robustness` | malformed/edge inputs do not crash the agent's OpenCV, and metamorphic invariants (compare symmetry, QR encode→decode round-trip) hold |

## Status

**Built and run in Docker** (`./scripts/run.sh docker guess
--cfg=localhost`) against an **OpenCV 4.14.0 built from source with
QUIRC** and the barcode module (the suite's Dockerfile builds it; Debian's
stock libopencv lacks QUIRC and is too old for the barcode module), plus
libtesseract. **All fourteen iterations pass live** against the real
libraries — including QR decoding (one and two symbols), OCR whole and by
region, format/depth sniffing, the diff heatmap, a multi-location template
match, the realistic end-to-end screenshot, the robustness group, and the
barcode control. The run is green against expectations
(`Passed, as expected: 14`, no unexpected results).

## Notes on two tests

- **`barcode`** asserts against OpenCV's own reference EAN-13 photos
  (`opencv_extra` `testdata/cv/barcode/single`), which decode to known
  values; a synthetic `python-barcode` symbol is decoded alongside as a
  non-fatal diagnostic, since crisp vector bars on flat white are not what
  `cv::barcode`'s localisation is tuned for.
- **`match`** uses `TM_SQDIFF` for a solid-colour template: the normalised
  methods (`TM_CCOEFF_NORMED` / `TM_CCORR_NORMED`) divide by the template's
  standard deviation, which is zero for a uniform template, so they are not
  usable there.

## Rebuilding here

`conf/external.yml`'s `url` points at the local tsf-cv checkout, because
the Docker build container has no git credentials for the private github
repository; the run mounts the parent directory, so the sibling checkout
is visible inside the container. On CI or a host with credentials, switch
`url` back to the github URL and `ref: main`. A run rewrites
`conf/builder.conf.lock` pinning the resolved commit; reset it to
header-only (`grep '^#' conf/builder.conf.lock > x && mv x
conf/builder.conf.lock`) and clear `/tmp/aibuild /tmp/te_ws_*` to pick up
a new module commit.
