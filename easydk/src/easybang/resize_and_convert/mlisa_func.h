/*************************************************************************
 * Copyright (C) [2018] by Cambricon, Inc.
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *************************************************************************/
#ifndef MLISA_FUNC_H
#define MLISA_FUNC_H

#define PERF_START()                            \
  __asm__ volatile("readperf.begin;\n\t");      \

#define PERF_END()                              \
  __sync_all();                                 \
  __asm__ volatile("readperf.end;\n\t");        \

__mlu_func__ void loadConst2Nram(half *dst, half* src, int size) {
  __asm__ volatile(
      "ld.nram.const [%[dst]], [%[src]], %[size];\n\t"
      :
      : [dst] "r"(dst), [src] "r"(src), [size] "r"(size));
}

__mlu_func__ void loadConst2Wram(half *dst, half* src, int size) {
  __asm__ volatile(
      "ld.wram.const [%[dst]], [%[src]], %[size];\n\t"
      :
      : [dst] "r"(dst), [src] "r"(src), [size] "r"(size));
}

#endif
