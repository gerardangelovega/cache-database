const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const server_source_files: []const []const u8 = &.{
        "src/server/avl.cpp",
        "src/server/buffer.cpp",
        "src/server/common.cpp",
        "src/server/conn.cpp",
        "src/server/data.cpp",
        "src/server/hashtable.cpp",
        "src/server/main.cpp",
        "src/server/serialize.cpp",
        "src/server/zset.cpp",
        "src/server/time.cpp",
        "src/server/thread_pool.cpp",
        "src/server/heap.cpp",
    };
    const client_source_files: []const []const u8 = &.{
        "src/client/main.cpp",
    };
    const shared_source_files: []const []const u8 = &.{
        "src/shared/error.cpp",
        "src/shared/io.cpp",
    };
    const flags: []const []const u8 = &.{
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        "-Wno-zero-length-array",
        "-Wno-gnu",
        "-std=gnu++17",
    };

    const server_exe = b.addExecutable(.{
        .name = "redstonedb_server",
        .root_module = b.createModule(.{
            .root_source_file = null,
            .target = target,
            .optimize = optimize,
            .link_libc = true,
            .link_libcpp = true,
        }),
    });
    server_exe.root_module.addCSourceFiles(.{ .files = server_source_files, .flags = flags });
    server_exe.root_module.addCSourceFiles(.{ .files = shared_source_files, .flags = flags });
    server_exe.root_module.addIncludePath(b.path("include/"));
    b.installArtifact(server_exe);

    const client_exe = b.addExecutable(.{ .name = "redstonedb_client", .root_module = b.createModule(.{ .root_source_file = null, .target = target, .optimize = optimize, .link_libc = true, .link_libcpp = true }) });
    client_exe.root_module.addCSourceFiles(.{
        .files = client_source_files,
        .flags = flags,
    });
    client_exe.root_module.addCSourceFiles(.{
        .files = shared_source_files,
        .flags = flags,
    });
    client_exe.root_module.addIncludePath(b.path("include/"));
    b.installArtifact(client_exe);

    const run_server_exe = b.addRunArtifact(server_exe);
    run_server_exe.step.dependOn(b.getInstallStep());

    const run_server_step = b.step("run-server", "Run the server");
    run_server_step.dependOn(&run_server_exe.step);

    const run_client_exe = b.addRunArtifact(client_exe);
    run_client_exe.step.dependOn(b.getInstallStep());
}
