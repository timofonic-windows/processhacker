﻿/*
 * Process Hacker Toolchain - 
 *   Build script
 * 
 * Copyright (C) 2017 dmex
 * 
 * This file is part of Process Hacker.
 * 
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Security.Cryptography;
using System.Text;

namespace CustomBuildTool
{
    [System.Security.SuppressUnmanagedCodeSecurity]
    public static class Win32
    {
        public static string ShellExecute(string FileName, string args)
        {
            string output = string.Empty;
            using (Process process = Process.Start(new ProcessStartInfo
            {
                UseShellExecute = false,
                RedirectStandardOutput = true,
                FileName = FileName,
                CreateNoWindow = true
            }))
            {
                process.StartInfo.Arguments = args;
                process.Start();

                output = process.StandardOutput.ReadToEnd();
                output = output.Replace("\n\n", "\r\n").Trim();

                process.WaitForExit();
            }

            return output;
        }

        public static void ImageResizeFile(int size, string FileName, string OutName)
        {
            using (var src = System.Drawing.Image.FromFile(FileName))
            using (var dst = new System.Drawing.Bitmap(size, size))
            using (var g = System.Drawing.Graphics.FromImage(dst))
            {
                g.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
                g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;

                g.DrawImage(src, 0, 0, dst.Width, dst.Height);

                dst.Save(OutName, System.Drawing.Imaging.ImageFormat.Png);
            }
        }

        public static void CopyIfNewer(string CurrentFile, string NewFile)
        {
            if (!File.Exists(CurrentFile))
                return;

            if (CurrentFile.EndsWith(".sys", StringComparison.OrdinalIgnoreCase))
            {
                if (!File.Exists(NewFile))
                {
                    File.Copy(CurrentFile, NewFile, true);
                }
                else
                {
                    FileVersionInfo currentInfo = FileVersionInfo.GetVersionInfo(CurrentFile);
                    FileVersionInfo newInfo = FileVersionInfo.GetVersionInfo(NewFile);
                    var currentInfoVersion = new Version(currentInfo.FileVersion);
                    var newInfoVersion = new Version(newInfo.FileVersion);

                    if (
                        currentInfoVersion > newInfoVersion ||
                        File.GetLastWriteTime(CurrentFile) > File.GetLastWriteTime(NewFile)
                        )
                    {
                        File.Copy(CurrentFile, NewFile, true);
                    }
                }
            }
            else
            {
                if (File.GetLastWriteTime(CurrentFile) > File.GetLastWriteTime(NewFile))
                    File.Copy(CurrentFile, NewFile, true);
            }
        }

        public static readonly IntPtr INVALID_HANDLE_VALUE = new IntPtr(-1);
        public const int STD_OUTPUT_HANDLE = -11;
        public const int STD_INPUT_HANDLE = -10;
        public const int STD_ERROR_HANDLE = -12;

        [DllImport("kernel32.dll", ExactSpelling = true)]
        public static extern IntPtr GetStdHandle(int StdHandle);
        [DllImport("kernel32.dll", ExactSpelling = true)]
        public static extern bool GetConsoleMode(IntPtr ConsoleHandle, out ConsoleMode Mode);
        [DllImport("kernel32.dll", ExactSpelling = true)]
        public static extern bool SetConsoleMode(IntPtr ConsoleHandle, ConsoleMode Mode);
        [DllImport("kernel32.dll", ExactSpelling = true)]
        public static extern IntPtr GetConsoleWindow();
    }

    public static class Json<T> where T : class
    {
        public static string Serialize(T instance)
        {
            DataContractJsonSerializer serializer = new DataContractJsonSerializer(typeof(T));

            using (MemoryStream stream = new MemoryStream())
            {
                serializer.WriteObject(stream, instance);

                return Encoding.Default.GetString(stream.ToArray());
            }
        }

        public static T DeSerialize(string json)
        {
            using (MemoryStream stream = new MemoryStream(Encoding.Default.GetBytes(json)))
            {
                DataContractJsonSerializer serializer = new DataContractJsonSerializer(typeof(T));

                return (T)serializer.ReadObject(stream);
            }
        }
    }

    public static class Verify
    {
        private static Rijndael GetRijndael(string secret)
        {
            Rfc2898DeriveBytes rfc2898DeriveBytes = new Rfc2898DeriveBytes(secret, Convert.FromBase64String("e0U0RTY2RjU5LUNBRjItNEMzOS1BN0Y4LTQ2MDk3QjFDNDYxQn0="), 10000);
            Rijndael rijndael = Rijndael.Create();
            rijndael.Key = rfc2898DeriveBytes.GetBytes(32);
            rijndael.IV = rfc2898DeriveBytes.GetBytes(16);
            return rijndael;
        }

        public static void Encrypt(string fileName, string outFileName, string secret)
        {
            FileStream fileOutStream = File.Create(outFileName);

            using (Rijndael rijndael = GetRijndael(secret))
            using (FileStream fileStream = File.OpenRead(fileName))
            using (CryptoStream cryptoStream = new CryptoStream(fileOutStream, rijndael.CreateEncryptor(), CryptoStreamMode.Write))
            {
                fileStream.CopyTo(cryptoStream);
            }
        }

        public static void Decrypt(string FileName, string outFileName, string secret)
        {
            FileStream fileOutStream = File.Create(outFileName);

            using (Rijndael rijndael = GetRijndael(secret))
            using (FileStream fileStream = File.OpenRead(FileName))
            using (CryptoStream cryptoStream = new CryptoStream(fileOutStream, rijndael.CreateDecryptor(), CryptoStreamMode.Write))
            {
                fileStream.CopyTo(cryptoStream);
            }
        }

        public static string HashFile(string FileName)
        {
            //using (HashAlgorithm algorithm = new SHA256CryptoServiceProvider())
            //{
            //    byte[] inputBytes = Encoding.UTF8.GetBytes(input);
            //    byte[] hashBytes = algorithm.ComputeHash(inputBytes);
            //    return BitConverter.ToString(hashBytes).Replace("-", String.Empty);
            //}

            FileStream fileInStream = File.OpenRead(FileName);

            using (BufferedStream bufferedStream = new BufferedStream(fileInStream, 0x1000))
            {
                SHA256Managed sha = new SHA256Managed();
                byte[] checksum = sha.ComputeHash(bufferedStream);

                return BitConverter.ToString(checksum).Replace("-", string.Empty);
            }
        }
    }

    public static class VisualStudio
    {
        public static string GetMsbuildFilePath()
        {
            string vswhere = Environment.ExpandEnvironmentVariables("%ProgramFiles(x86)%\\Microsoft Visual Studio\\Installer\\vswhere.exe");

            // Note: vswere.exe was only released with build 15.0.26418.1
            if (File.Exists(vswhere))
            {
                string vswhereResult = Win32.ShellExecute(vswhere,
                    "-latest " +
                    "-products * " +
                    "-requires Microsoft.Component.MSBuild " +
                    "-property installationPath "
                    );

                if (string.IsNullOrEmpty(vswhereResult))
                    return null;

                if (File.Exists(vswhereResult + "\\MSBuild\\15.0\\Bin\\MSBuild.exe"))
                    return vswhereResult + "\\MSBuild\\15.0\\Bin\\MSBuild.exe";

                return null;
            }
            else
            {
                try
                {
                    VisualStudioInstance instance = FindVisualStudioInstance();

                    if (instance != null)
                    {
                        if (File.Exists(instance.Path + "\\MSBuild\\15.0\\Bin\\MSBuild.exe"))
                            return instance.Path + "\\MSBuild\\15.0\\Bin\\MSBuild.exe";
                    }
                }
                catch (Exception ex)
                {
                    Program.PrintColorMessage("[VisualStudioInstance] " + ex, ConsoleColor.Red, true);
                }

                return null;
            }
        }

        private static VisualStudioInstance FindVisualStudioInstance()
        {
            var setupConfiguration = new SetupConfiguration() as ISetupConfiguration2;
            var instanceEnumerator = setupConfiguration.EnumAllInstances();
            var instances = new ISetupInstance2[3];

            instanceEnumerator.Next(instances.Length, instances, out var instancesFetched);

            if (instancesFetched == 0)
                return null;

            do
            {
                for (int i = 0; i < instancesFetched; i++)
                {
                    var instance = new VisualStudioInstance(instances[i]);
                    var state = instances[i].GetState();
                    var packages = instances[i].GetPackages().Where(package => package.GetId().Contains("Microsoft.Component.MSBuild"));

                    if (
                        state.HasFlag(InstanceState.Local | InstanceState.Registered | InstanceState.Complete) &&
                        packages.Count() > 0 &&
                        instance.Version.StartsWith("15.0", StringComparison.OrdinalIgnoreCase)
                        )
                    {
                        return instance;
                    }
                }

                instanceEnumerator.Next(instances.Length, instances, out instancesFetched);
            }
            while (instancesFetched != 0);

            return null;
        }
    }

    public class VisualStudioInstance
    {
        public bool IsLaunchable { get; }
        public bool IsComplete { get; }
        public string Name { get; }
        public string Path { get; }
        public string Version { get; }
        public string DisplayName { get; }
        public string Description { get; }
        public string ResolvePath { get; }
        public string EnginePath { get; }
        public string ProductPath { get; }
        public string InstanceId { get; }
        public DateTime InstallDate { get; }

        public VisualStudioInstance(ISetupInstance2 FromInstance)
        {
            this.IsLaunchable = FromInstance.IsLaunchable();
            this.IsComplete = FromInstance.IsComplete();
            this.Name = FromInstance.GetInstallationName();
            this.Path = FromInstance.GetInstallationPath();
            this.Version = FromInstance.GetInstallationVersion();
            this.DisplayName = FromInstance.GetDisplayName();
            this.Description = FromInstance.GetDescription();
            this.ResolvePath = FromInstance.ResolvePath();
            this.EnginePath = FromInstance.GetEnginePath();
            this.InstanceId = FromInstance.GetInstanceId();
            this.ProductPath = FromInstance.GetProductPath();

            try
            {
                var time = FromInstance.GetInstallDate();
                ulong high = (ulong)time.dwHighDateTime;
                uint low = (uint)time.dwLowDateTime;
                long fileTime = (long)((high << 32) + low);

                this.InstallDate = DateTime.FromFileTimeUtc(fileTime);
            }
            catch
            {
                this.InstallDate = DateTime.UtcNow;
            }

            // FromInstance.GetState();
            // FromInstance.GetPackages();
            // FromInstance.GetProduct();            
            // FromInstance.GetProperties();
            // FromInstance.GetErrors();
        }
    }

    [DataContract]
    public class BuildUpdateRequest
    {
        [DataMember(Name = "version")] public string Version { get; set; }
        [DataMember(Name = "commit")] public string Commit { get; set; }
        [DataMember(Name = "updated")] public string Updated { get; set; }

        [DataMember(Name = "bin_url")] public string BinUrl { get; set; }
        [DataMember(Name = "bin_length")] public string BinLength { get; set; }
        [DataMember(Name = "bin_hash")] public string BinHash { get; set; }
        [DataMember(Name = "bin_sig")] public string BinSig { get; set; }

        [DataMember(Name = "setup_url")] public string SetupUrl { get; set; }
        [DataMember(Name = "setup_length")] public string SetupLength { get; set; }
        [DataMember(Name = "setup_hash")] public string SetupHash { get; set; }
        [DataMember(Name = "setup_sig")] public string SetupSig { get; set; }

        [DataMember(Name = "websetup_url")] public string WebSetupUrl { get; set; }
        [DataMember(Name = "websetup_version")] public string WebSetupVersion { get; set; }
        [DataMember(Name = "websetup_length")] public string WebSetupLength { get; set; }
        [DataMember(Name = "websetup_hash")] public string WebSetupHash { get; set; }
        [DataMember(Name = "websetup_sig")] public string WebSetupSig { get; set; }

        [DataMember(Name = "message")] public string Message { get; set; }
        [DataMember(Name = "changelog")] public string Changelog { get; set; }

        [DataMember(Name = "size")] public string FileLengthDeprecated { get; set; } // TODO: Remove after most users have updated.
        [DataMember(Name = "forum_url")] public string ForumUrlDeprecated { get; set; } // TODO: Remove after most users have updated.
        [DataMember(Name = "hash_bin")] public string BinHashDeprecated { get; set; } // TODO: Remove after most users have updated.
        [DataMember(Name = "hash_setup")] public string SetupHashDeprecated { get; set; } // TODO: Remove after most users have updated.
        [DataMember(Name = "sig")] public string SetupSigDeprecated { get; set; } // TODO: Remove after most users have updated.
    }

    [Flags]
    public enum ConsoleMode : uint
    {
        DEFAULT,
        ENABLE_PROCESSED_INPUT = 0x0001,
        ENABLE_LINE_INPUT = 0x0002,
        ENABLE_ECHO_INPUT = 0x0004,
        ENABLE_WINDOW_INPUT = 0x0008,
        ENABLE_MOUSE_INPUT = 0x0010,
        ENABLE_INSERT_MODE = 0x0020,
        ENABLE_QUICK_EDIT_MODE = 0x0040,
        ENABLE_EXTENDED_FLAGS = 0x0080,
        ENABLE_AUTO_POSITION = 0x0100,
        ENABLE_PROCESSED_OUTPUT = 0x0001,
        ENABLE_WRAP_AT_EOL_OUTPUT = 0x0002,
        ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004,
        DISABLE_NEWLINE_AUTO_RETURN = 0x0008,
        ENABLE_LVB_GRID_WORLDWIDE = 0x0010,
    }

    [Flags]
    public enum InstanceState : uint
    {
        None = 0u,
        Local = 1u,
        Registered = 2u,
        NoRebootRequired = 4u,
        NoErrors = 8u,
        Complete = 4294967295u
    }
}
