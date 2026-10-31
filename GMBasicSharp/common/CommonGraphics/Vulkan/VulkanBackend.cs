// GPU/Backends/VulkanBackend.cs — GPU rendering via Silk.NET Vulkan

using System.Diagnostics;
using System.Runtime.InteropServices;
using Silk.NET.Vulkan;
using Silk.NET.Vulkan.Extensions.KHR;

namespace CommonGraphics.Vulkan;

using VkSem = Silk.NET.Vulkan.Semaphore;

public unsafe sealed class VulkanBackend : IGraphicsBackend
{
    private static void Check(Result r) { if (r != Result.Success) throw new InvalidOperationException($"Vulkan error: {r}"); }
    private static Vk _vk = null!;
    private Instance _instance;
    private PhysicalDevice _physicalDevice;
    private Device _device;
    private Queue _queue;
    private uint _queueFamily;
    private CommandPool _cmdPool;
    private readonly Dictionary<IntPtr, Tex> _textures = new();
    private int _texId;
    private bool _init;
    private string _devName = "?";

    public string Name => $"Vulkan ({_devName})";
    public bool IsAvailable => NativeLibrary.TryLoad(
        RuntimeInformation.IsOSPlatform(OSPlatform.OSX) ? "libMoltenVK.dylib" : "vulkan-1.dll",
        out _);

    public void Initialize()
    {
        if (_init) return;
        _vk = Vk.GetApi()!;

        // Instance
        var appInfo = new ApplicationInfo { SType = StructureType.ApplicationInfo, ApiVersion = Vk.Version13 };
        byte* extName = (byte*)Marshal.StringToHGlobalAnsi("VK_KHR_surface");
        var instCI = new InstanceCreateInfo { SType = StructureType.InstanceCreateInfo,
            PApplicationInfo = &appInfo, EnabledExtensionCount = 1, PpEnabledExtensionNames = &extName };
        _vk.CreateInstance(&instCI, null, out _instance);

        // Physical device
        uint devCount = 0;
        _vk.EnumeratePhysicalDevices(_instance, &devCount, null);
        var devs = new PhysicalDevice[devCount];
        fixed (PhysicalDevice* p = devs) _vk.EnumeratePhysicalDevices(_instance, &devCount, p);
        _physicalDevice = devs[0];
        PhysicalDeviceProperties props;
        _vk.GetPhysicalDeviceProperties(_physicalDevice, &props);
        _devName = Marshal.PtrToStringAnsi((nint)props.DeviceName) ?? "?";

        // Queue family
        uint qfCount = 0;
        _vk.GetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &qfCount, null);
        var qfs = new QueueFamilyProperties[qfCount];
        fixed (QueueFamilyProperties* p = qfs) _vk.GetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &qfCount, p);
        for (uint i = 0; i < qfCount; i++)
            if (qfs[i].QueueFlags.HasFlag(QueueFlags.GraphicsBit)) { _queueFamily = i; break; }

        // Device
        float prio = 1f;
        var qCI = new DeviceQueueCreateInfo { SType = StructureType.DeviceQueueCreateInfo,
            QueueFamilyIndex = _queueFamily, QueueCount = 1, PQueuePriorities = &prio };
        var devCI = new DeviceCreateInfo { SType = StructureType.DeviceCreateInfo,
            QueueCreateInfoCount = 1, PQueueCreateInfos = &qCI };
        _vk.CreateDevice(_physicalDevice, &devCI, null, out _device);
        _vk.GetDeviceQueue(_device, _queueFamily, 0, out _queue);

        // Command pool
        var poolCI = new CommandPoolCreateInfo { SType = StructureType.CommandPoolCreateInfo,
            Flags = CommandPoolCreateFlags.ResetCommandBufferBit, QueueFamilyIndex = _queueFamily };
        _vk.CreateCommandPool(_device, &poolCI, null, out _cmdPool);

        _init = true;
        Trace.WriteLine($"[Vulkan] {_devName} ready");
    }

    public void DrawSprites(IntPtr texId, float[] positions, float[] uvs, int count)
    {
        if (!_init || count == 0) return;
        // Allocate single-use command buffer
        var allocInfo = new CommandBufferAllocateInfo { SType = StructureType.CommandBufferAllocateInfo,
            CommandPool = _cmdPool, Level = CommandBufferLevel.Primary, CommandBufferCount = 1 };
        CommandBuffer cb;
        _vk.AllocateCommandBuffers(_device, &allocInfo, &cb);

        var beginInfo = new CommandBufferBeginInfo { SType = StructureType.CommandBufferBeginInfo,
            Flags = CommandBufferUsageFlags.OneTimeSubmitBit };
        _vk.BeginCommandBuffer(cb, &beginInfo);

        // Bind sprite pipeline
        var pipeline = GetOrCreateSpritePipeline();
        _vk.CmdBindPipeline(cb, PipelineBindPoint.Graphics, pipeline);

        // TODO: vertex buffer + descriptor set + vkCmdDraw
        // For now: placeholder that just records an empty command buffer

        _vk.EndCommandBuffer(cb);

        var submitInfo = new SubmitInfo { SType = StructureType.SubmitInfo,
            CommandBufferCount = 1, PCommandBuffers = &cb };
        _vk.QueueSubmit(_queue, 1, &submitInfo, default);
        _vk.QueueWaitIdle(_queue);
        _vk.FreeCommandBuffers(_device, _cmdPool, 1, &cb);
    }

    public void PresentFrame() { /* requires swapchain + surface */ }

    public IntPtr CreateTexture(int width, int height, byte[] rgba)
    {
        var h = (IntPtr)Interlocked.Increment(ref _texId);
        _textures[h] = new Tex();
        // TODO: vkCreateImage + upload
        return h;
    }

    public void DestroyTexture(IntPtr id) { _textures.Remove(id); }
    public void ClearScreen(float r, float g, float b, float a) { }

    public void Dispose()
    {
        if (!_init) return;
        _vk.DestroyCommandPool(_device, _cmdPool, null);
        _vk.DestroyDevice(_device, null);
        _vk.DestroyInstance(_instance, null);
        _vk.Dispose();
        _init = false;
    }

    // ---- Pipeline ----

    private Pipeline _spritePipeline;
    private PipelineLayout _pipelineLayout;

    private Pipeline GetOrCreateSpritePipeline()
    {
        if (_spritePipeline.Handle != 0) return _spritePipeline;

        // Layout
        var layoutCI = new PipelineLayoutCreateInfo { SType = StructureType.PipelineLayoutCreateInfo };
        _vk.CreatePipelineLayout(_device, &layoutCI, null, out _pipelineLayout);

        // Shaders
        var vert = LoadSpirv("GPU.Shaders.sprite.vert.spv");
        var frag = LoadSpirv("GPU.Shaders.sprite.frag.spv");
        var vertMod = CreateShaderModule(vert);
        var fragMod = CreateShaderModule(frag);

        var vertStage = new PipelineShaderStageCreateInfo { SType = StructureType.PipelineShaderStageCreateInfo,
            Stage = ShaderStageFlags.VertexBit, Module = vertMod, PName = (byte*)Marshal.StringToHGlobalAnsi("main") };
        var fragStage = new PipelineShaderStageCreateInfo { SType = StructureType.PipelineShaderStageCreateInfo,
            Stage = ShaderStageFlags.FragmentBit, Module = fragMod, PName = (byte*)Marshal.StringToHGlobalAnsi("main") };
        var stages = stackalloc PipelineShaderStageCreateInfo[] { vertStage, fragStage };

        // Dynamic states
        var dynStates = stackalloc DynamicState[] { DynamicState.Viewport, DynamicState.Scissor };
        var dynCI = new PipelineDynamicStateCreateInfo { SType = StructureType.PipelineDynamicStateCreateInfo,
            DynamicStateCount = 2, PDynamicStates = dynStates };

        // Vertex input (empty for now — sprite vertices pushed as constants or SSBO)
        var vertexCI = new PipelineVertexInputStateCreateInfo { SType = StructureType.PipelineVertexInputStateCreateInfo };

        var inputAssembly = new PipelineInputAssemblyStateCreateInfo { SType = StructureType.PipelineInputAssemblyStateCreateInfo,
            Topology = PrimitiveTopology.TriangleList };

        var viewportCI = new PipelineViewportStateCreateInfo { SType = StructureType.PipelineViewportStateCreateInfo,
            ViewportCount = 1, ScissorCount = 1 };

        var rasterizer = new PipelineRasterizationStateCreateInfo { SType = StructureType.PipelineRasterizationStateCreateInfo,
            LineWidth = 1f, PolygonMode = PolygonMode.Fill, CullMode = CullModeFlags.None, 
            FrontFace = FrontFace.CounterClockwise };

        var multisampling = new PipelineMultisampleStateCreateInfo { SType = StructureType.PipelineMultisampleStateCreateInfo,
            RasterizationSamples = SampleCountFlags.Count1Bit };

        var colorBlend = new PipelineColorBlendAttachmentState { ColorWriteMask = (ColorComponentFlags)0xF };
        var blendCI = new PipelineColorBlendStateCreateInfo { SType = StructureType.PipelineColorBlendStateCreateInfo,
            AttachmentCount = 1, PAttachments = &colorBlend };

        var pipelineCI = new GraphicsPipelineCreateInfo { SType = StructureType.GraphicsPipelineCreateInfo,
            StageCount = 2, PStages = stages, PVertexInputState = &vertexCI, PInputAssemblyState = &inputAssembly,
            PViewportState = &viewportCI, PRasterizationState = &rasterizer, PMultisampleState = &multisampling,
            PColorBlendState = &blendCI, PDynamicState = &dynCI, Layout = _pipelineLayout,
            RenderPass = default, Subpass = 0 };

        _vk.CreateGraphicsPipelines(_device, default, 1, &pipelineCI, null, out _spritePipeline);

        _vk.DestroyShaderModule(_device, vertMod, null);
        _vk.DestroyShaderModule(_device, fragMod, null);
        return _spritePipeline;
    }

    private ShaderModule CreateShaderModule(byte[] spirv)
    {
        fixed (byte* p = spirv)
        {
            var ci = new ShaderModuleCreateInfo { SType = StructureType.ShaderModuleCreateInfo,
                CodeSize = (nuint)spirv.Length, PCode = (uint*)p };
            _vk.CreateShaderModule(_device, &ci, null, out var mod);
            return mod;
        }
    }

    private static byte[] LoadSpirv(string name)
    {
        using var s = typeof(VulkanBackend).Assembly.GetManifestResourceStream(name);
        if (s == null) return [];  // shader not embedded — compile .vert → .spv first
        var data = new byte[s.Length];
        s.ReadExactly(data);
        return data;
    }

    private sealed class Tex { }
}
