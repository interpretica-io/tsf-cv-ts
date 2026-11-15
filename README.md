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

## Status

**Built and green in Docker** (`./scripts/run.sh docker guess
--cfg=localhost`) against OpenCV 4.6.0 on Debian bookworm: `info`,
`compare`, `match`, `region` and `ocr` pass against the real libraries,
and `decode` skips because that OpenCV build has no QUIRC, so QR
decoding is unavailable (the agent reports no QR backend). The run
finishes `PASSED`; `conf/trc.xml` records the `decode` skip as expected.
On an OpenCV built with QUIRC the `decode` iteration passes too.

To rebuild here before tsf-cv is published, point `conf/external.yml`'s
`url` at a local checkout of tsf-cv (an absolute path git can clone)
and pin its commit in `conf/builder.conf.lock`; the Docker run mounts
the parent directory, so a sibling `../tsf-cv` is visible inside the
container.
