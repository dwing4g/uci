// tested on JRE6

public final class UCIDec
{
	public native static int UCIDecode(byte[] src, int srclen, byte[][] dst, int[] stride, int[] w, int[] h, int[] b);

	public native static void UCIDebug(int level);

	static
	{
		System.load(new java.io.File(UCIDec.class.getResource("ucidec.dll").getFile()).getAbsolutePath());
	}

	public static void main(String[] args) throws Exception
	{
		final java.io.FileInputStream fis = new java.io.FileInputStream("test.uci");
		final byte[] src = new byte[(int)fis.getChannel().size()];
		fis.read(src);
		fis.close();

		UCIDebug(0x7fffffff);
		final byte[][] dst = new byte[1][];
		final int[] stride = new int[1];
		final int[] w = new int[1];
		final int[] h = new int[1];
		final int[] b = new int[1];
		final int r = UCIDecode(src, src.length, dst, stride, w, h, b);
		if(r < 0)
		{
			System.err.println("ERROR: DecodeUCI failed (return " + r + ")");
			return;
		}
		System.err.println("INFO: width x height x bit : " + w[0] + " x " + h[0] + " x " + b[0]);

		final java.io.FileOutputStream fos = new java.io.FileOutputStream("test.rgb");
		for(int i = 0; i < h[0]; ++i)
			fos.write(dst[0], i * stride[0], w[0] * (b[0] / 8));
		fos.close();
	}
}
