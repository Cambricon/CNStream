/*copyright (C) [2019] by Cambricon, Inc.
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
#ifndef _MLU270_OPS_CROPANDRESIZEYUV2YUV_KERNEL_H_
#define _MLU270_OPS_CROPANDRESIZEYUV2YUV_KERNEL_H_

#include <cstdint>

void MLUUnion1KernelResizeYuv420sp(
    uint32_t s_row, uint32_t s_col, uint32_t s_stride_y, uint32_t s_stride_uv,
    void* Ysrc_gdram, void* UVsrc_gdram,
    uint32_t d_row, uint32_t d_col, void* dst_y, void* dst_uv,
    uint32_t batch);

#endif

