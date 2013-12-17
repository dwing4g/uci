using System;
using System.IO;
using System.Runtime.InteropServices;

namespace TestUCI
{
	class Program
	{
		[DllImport("ucidec.dll")]
		public static extern int UCIDecode(byte[] src, int srclen, out IntPtr dst, out int stride, out int w, out int h, out int b);
		[DllImport("ucidec.dll")]
		public static extern int UCIFree(IntPtr p);
		[DllImport("ucidec.dll")]
		public static extern int UCIDebug(int level);

		private static void Main(string[] args)
		{
			byte[] src;
			IntPtr dst;
			int w, h, b, stride, r;
			FileStream fs;

			using(fs = new FileStream("test.uci", FileMode.Open))
			{
				src = new byte[fs.Length];
				fs.Read(src, 0, (int) fs.Length);
				fs.Close();
			}

			UCIDebug(0x7fffffff);
			r = UCIDecode(src, src.Length, out dst, out stride, out w, out h, out b);
			if(r < 0)
			{
				Console.WriteLine("ERROR: DecodeUCI failed (return " + r + ")");
				return;
			}
			Console.WriteLine("INFO: width x height x bit : " + w + " x " + h + " x " + b);

			using(fs = new FileStream("test.rgb", FileMode.OpenOrCreate))
			{
				src = new byte[w * (b/8)];
				for(int i = 0; i < h; ++i)
				{
					Marshal.Copy(new IntPtr(dst.ToInt32() + i * stride), src, 0, src.Length);
					fs.Write(src, 0, src.Length);
				}
				fs.Close();
			}

			UCIFree(dst);
		}
	}
}
