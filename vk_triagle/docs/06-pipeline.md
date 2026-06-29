# 6. Graphics Pipeline

## 6.1 Konsep Pipeline

Graphics pipeline adalah **rantai tahapan** yang harus dilalui vertex
untuk menjadi pixel di layar:

```
┌─────────────────────────────────────────────────────────────────┐
│                    GRAPHICS PIPELINE                             │
│                                                                  │
│  Vertex Buffer → Input Assembler → Vertex Shader → Tessellation  │
│  (data mentah)    (baca vertex)     (posisi layar)   (opsional) │
│                                                                  │
│  → Geometry Shader → Rasterization → Fragment Shader → Blending │
│    (opsional)         (pixelisasi)    (warna pixel)    (output)  │
└─────────────────────────────────────────────────────────────────┘
```

Aplikasi kita cuma pakai:
- **Input Assembler** → baca 3 vertex
- **Vertex Shader** → hitung posisi layar
- **Rasterization** → jadi pixel
- **Fragment Shader** → kasih warna
- **Blending** → tulis ke framebuffer

## 6.2 Vertex Input

### Vertex Buffer (Data Kasar)

```cpp
struct Vertex { float pos[2]; float color[3]; };

const Vertex verts[] = {
    {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},  // kiri atas, merah
    {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},  // kanan bawah, hijau
    {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},  // kiri bawah, biru
};
```

Setiap vertex = 20 byte:
```
pos[0] pos[1]  color[0] color[1] color[2]
 0.0   -0.5    1.0      0.0      0.0        ← merah
 0.5    0.5    0.0      1.0      0.0        ← hijau
-0.5    0.5    0.0      0.0      1.0        ← biru
↑---- 8 byte ---↑↑------- 12 byte ---------↑
```

### Binding Description

Memberi tahu Vulkan **bagaimana data diatur dalam buffer**:

```cpp
VkVertexInputBindingDescription bind{};
bind.binding   = 0;              // binding index (0 = first buffer)
bind.stride    = 5 * sizeof(float);  // 20 byte per vertex
bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;  // setiap vertex, bukan instance
```

### Attribute Description

Memberi tahu Vulkan **bagaimana memetakan byte ke input shader**:

```cpp
// Location 0: position (vec2) — offset 0
attr[0].location = 0;
attr[0].binding  = 0;
attr[0].format   = VK_FORMAT_R32G32_SFLOAT;  // 2 x float32
attr[0].offset   = 0;

// Location 1: color (vec3) — offset 8
attr[1].location = 1;
attr[1].binding  = 0;
attr[1].format   = VK_FORMAT_R32G32B32_SFLOAT;  // 3 x float32
attr[1].offset   = 2 * sizeof(float);
```

**Visualisasi mapping:**

```
Vertex buffer (memory):
  [0.0][-0.5][1.0][0.0][0.0] [0.5][0.5][0.0][1.0][0.0] [-0.5][0.5][0.0][0.0][1.0]
   ↑ offset 0                  ↑ offset 20                ↑ offset 40
   └── attr[0] pos             └── attr[0] pos            └── attr[0] pos
   └──── attr[1] color         └──── attr[1] color        └──── attr[1] color

Setelah vertex shader:
  Vertex 0: position = (0.0, -0.5), color = (1.0, 0.0, 0.0)
  Vertex 1: position = (0.5,  0.5), color = (0.0, 1.0, 0.0)
  Vertex 2: position = (-0.5, 0.5), color = (0.0, 0.0, 1.0)
```

### Upload Vertex ke GPU

```cpp
// 1. Buat buffer di GPU
vkCreateBuffer(device, &bufferInfo, nullptr, &m_vertBuf);

// 2. Alokasikan memory HOST_VISIBLE (CPU bisa tulis)
VkMemoryRequirements mr;
vkGetBufferMemoryRequirements(device, m_vertBuf, &mr);

uint32_t memType = findMemoryType(mr.memoryTypeBits,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

vkAllocateMemory(device, &allocInfo, nullptr, &m_vertMem);
vkBindBufferMemory(device, m_vertBuf, m_vertMem, 0);

// 3. Copy data dari CPU ke GPU memory
void* data;
vkMapMemory(device, m_vertMem, 0, size, 0, &data);
memcpy(data, verts, size);          // ← copy vertex data
vkUnmapMemory(device, m_vertMem);
```

**`HOST_VISIBLE`** = CPU bisa akses langsung.
**`HOST_COHERENT`** = CPU write langsung kelihatan GPU (tanpa flush).

## 6.3 Shader Stages

### Membaca SPIR-V

```cpp
auto vertCode = loadSPIRV("shaders_spv/triangle.vert.spv");
// vertCode = array uint32_t (600 byte → 150 uint32_t)

VkShaderModule vertMod;

VkShaderModuleCreateInfo ci{};
ci.codeSize = vertCode.size() * sizeof(uint32_t);  // 600 byte
ci.pCode    = vertCode.data();                      // pointer ke SPIR-V

vkCreateShaderModule(device, &ci, nullptr, &vertMod);
```

### Memasang ke Pipeline

```cpp
VkPipelineShaderStageCreateInfo stages[2]{};

// Vertex stage
stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
stages[0].module = vertMod;
stages[0].pName  = "vs_main";   // ← nama fungsi di HLSL

// Fragment stage
stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
stages[1].module = fragMod;
stages[1].pName  = "ps_main";   // ← nama fungsi di HLSL
```

**Nama entry point harus cocok** antara pipeline dan shader:
- Pipeline cari `"vs_main"` → HLSL punya fungsi bernama `vs_main`
- Pipeline cari `"ps_main"` → HLSL punya fungsi bernama `ps_main`

## 6.4 Input Assembly

```cpp
VkPipelineInputAssemblyStateCreateInfo ia{};
ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
```

| Topology | Arti | Jumlah Vertex |
|----------|------|---------------|
| `POINT_LIST` | Titik | setiap 1 vertex = 1 titik |
| `LINE_LIST` | Garis | setiap 2 vertex = 1 garis |
| `TRIANGLE_LIST` | Segitiga | setiap 3 vertex = 1 segitiga |
| `TRIANGLE_STRIP` | Strip segitiga | N vertex = N-2 segitiga |

Kita pake `TRIANGLE_LIST` dengan 3 vertex → 1 segitiga.

## 6.5 Viewport & Scissor

**Viewport** = area framebuffer di mana hasil render akan diperbesar.

```cpp
VkViewport vp{};
vp.x = 0;        vp.y = 0;
vp.width  = m_swapExtent.width;     // lebar window
vp.height = m_swapExtent.height;    // tinggi window
vp.minDepth = 0.0f;   // depth = 0 (dekat)
vp.maxDepth = 1.0f;   // depth = 1 (jauh)
```

Koordinat vertex (-1, -1) sampai (1, 1) akan dipetakan ke:
```
(-1,-1) → pojok kiri atas viewport = (0, 0)
( 1, 1) → pojok kanan bawah viewport = (width, height)
```

**Scissor** = area clipping. Pixel di luar scissor tidak di-render.

```cpp
VkRect2D sc{};
sc.offset = {0, 0};
sc.extent = m_swapExtent;    // sama dengan viewport → no clipping
```

## 6.6 Rasterization State

```cpp
VkPipelineRasterizationStateCreateInfo rs{};
rs.polygonMode = VK_POLYGON_MODE_FILL;   // solid triangle
rs.cullMode    = VK_CULL_MODE_BACK_BIT;  // buang segitiga membelakangi
rs.frontFace   = VK_FRONT_FACE_CLOCKWISE; // CW = depan
rs.lineWidth   = 1.0f;                    // tebal garis (pixels)
```

### Front Face: Clockwise vs Counter-Clockwise

Vertex order menentukan mana depan dan belakang segitiga:

```
Clockwise (CW):               Counter-Clockwise (CCW):
  0───1                        0───2
  │  /                         │  /
  │ /                          │ /
  2                            1
  ↓ depan                      ↓ depan
```

Segitiga kita pakai CW:
```
Vertex 0  = ( 0.0, -0.5)   ← atas
Vertex 1  = ( 0.5,  0.5)   ← kanan bawah
Vertex 2  = (-0.5,  0.5)   ← kiri bawah

Dari 0 → 1 → 2: searah jarum jam → depan
```

## 6.7 Multisample & Color Blend

```cpp
VkPipelineMultisampleStateCreateInfo ms{};
ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;  // no MSAA

VkPipelineColorBlendAttachmentState cbAtt{};
cbAtt.blendEnable = VK_FALSE;                      // no blending
cbAtt.colorWriteMask = R_BIT | G_BIT | B_BIT | A_BIT;  // tulis semua channel
```

- **MSAA = 1** artinya tidak ada anti-aliasing.
- **Blending = false** artinya warna fragment langsung ditulis, tidak
  dicampur dengan warna sebelumnya.

## 6.8 Pipeline Layout

```cpp
VkPipelineLayoutCreateInfo pl{};
pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
// no push constants, no descriptor sets → kosong

vkCreatePipelineLayout(device, &pl, nullptr, &m_pipeLayout);
```

Pipeline layout mendefinisikan resource yang bisa diakses shader
(uniform buffer, sampler, dll). Untuk segitiga sederhana, kita tidak
punya resource tambahan.

## 6.9 Final: VkGraphicsPipelineCreateInfo

```cpp
VkGraphicsPipelineCreateInfo gi{};
gi.stageCount          = 2;                  // vertex + fragment
gi.pStages             = stages;
gi.pVertexInputState   = &vi;
gi.pInputAssemblyState = &ia;
gi.pViewportState      = &vs;
gi.pRasterizationState = &rs;
gi.pMultisampleState   = &ms;
gi.pColorBlendState    = &cb;
gi.layout              = m_pipeLayout;
gi.renderPass          = m_renderPass;
gi.subpass             = 0;

vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gi,
                          nullptr, &m_pipeline);
```

Setelah ini **shader module bisa di-destroy**:

```cpp
vkDestroyShaderModule(device, vertMod, nullptr);
vkDestroyShaderModule(device, fragMod, nullptr);
```

Pipeline sudah menyimpan salinan shader yang sudah dikompilasi.
