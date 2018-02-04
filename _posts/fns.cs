using System;
using System.Diagnostics;

delegate bool LessThanFn(int lhs, int rhs);

internal static class Program {
    private static int MaxInt(int[] arr, LessThanFn lt) {
        Debug.Assert(arr.Length > 0);
        int result = arr[0];
        for (int i = 1; i < arr.Length; ++i) {
            if (lt(result, arr[i]))
                result = arr[i];
        }
        return result;
    }

    private class DifferenceLess {
        public int Center { get; set; }

        public bool Compare(int lhs, int rhs) {
            return Math.Abs(lhs - Center) < Math.Abs(rhs - Center);
        }
    }

    public static void Main(string[] args) {
        int[] ints = {0, -1, 10, 4};
        var cmp = new DifferenceLess { Center = 11 };
        Debug.Assert(MaxInt(ints, cmp.Compare) == -1);
        cmp.Center = 4;
        Debug.Assert(MaxInt(ints, cmp.Compare) == 10);
    }
}
