class Solution {
    public int[] platesBetweenCandles(String s, int[][] queries) {
        int[] dish_toleft=new int[s.length()];
        int[] close_candle_left=new int[s.length()];
        int[] close_candle_right=new int[s.length()];
        int[] res=new int[queries.length];
        if(s.charAt(0)=='*'){
            dish_toleft[0]=1;
            close_candle_left[0]=-1;
        }
        else close_candle_left[0]=0;
        if(s.charAt(s.length()-1)=='*')
            close_candle_right[s.length()-1]=-1;
        else
            close_candle_right[s.length()-1]=s.length()-1;
        for(int i=1;i<s.length();i++)
        {
            if(s.charAt(i)=='*'){
                dish_toleft[i]=dish_toleft[i-1]+1;
                close_candle_left[i]=close_candle_left[i-1];
            }
            else{
                dish_toleft[i]=dish_toleft[i-1];
                close_candle_left[i]=i;
            }
             
        }
        for(int i=s.length()-2;i>=0;i--)
        {
            if(s.charAt(i)=='*')
                close_candle_right[i]=close_candle_right[i+1];
            else close_candle_right[i]=i;
        }
        for(int i=0;i<queries.length;i++)
        {
            if(close_candle_left[queries[i][1]]==-1 ||close_candle_right[queries[i][0]]==-1 || close_candle_left[queries[i][1]]<close_candle_right[queries[i][0]] )
                res[i]=0;
            else
            res[i]=dish_toleft[close_candle_left[queries[i][1]]]-dish_toleft[close_candle_right[queries[i][0]]];
        }
        return res;
    }
}

class Solution {
    public int numIdenticalPairs(int[] nums) {
        HashMap<Integer, Integer> count= new HashMap<>();
        int sum=0;
        for(int x: nums)
        {
            count.put(x, 1+count.getOrDefault(x,0));
        }
        for(Map.Entry<Integer, Integer> m: count.entrySet())
        {
            sum+=m.getValue()*(m.getValue()-1);
        }
        return sum/2;
    }
}

class Solution {
    public int minSetSize(int[] arr) {
        Map<Integer, Integer> map = new HashMap<>();
        int count[] = new int[arr.length+1];
        int goal=arr.length/2;
        int res = 0, sum = 0;
        for (int num : arr) {
            map.put(num, map.getOrDefault(num, 0) + 1);
        }
        for (Map.Entry<Integer, Integer> m : map.entrySet()) {
            count[m.getValue()]++;
        }
        for (int i = count.length - 1; i >= 0; i--) {
            while (count[i] > 0) {
                sum+=i;
                res++;
                count[i]--;
                if (sum >= goal)
                    return res;
            }
        }
        return res;
    }
}

class Solution {
    public int lastStoneWeight(int[] stones) {
        PriorityQueue<Integer> pq = new PriorityQueue<>(Collections.reverseOrder());
        for (int i = 0; i < stones.length; i++) {
            pq.offer(stones[i]);
        }
        while (pq.size() > 1) {
            pq.offer(Math.abs(pq.poll() - pq.poll()));
        }
        return pq.poll();
    }
}

class Solution {
    public int twoCitySchedCost(int[][] costs) {
        Arrays.sort(costs, (a,b)-> Math.abs(b[0]-b[1])-Math.abs(a[0]-a[1]));
        int a=0,b=0;
        int sum=0,n=costs.length/2;
        for(int[] c:costs)
        {
            if(a==n)
            {
                sum+=c[1]; 
            }
            if(b==n)
            {
                sum+=c[0];
            }
            if(a!=n&&b!=n)
            {
                if(c[0]<c[1])
                {
                    a++;
                    sum+=c[0];
                }
                else
                {
                    b++;
                    sum+=c[1]; 
                }
            }


        }
        return sum;
    }
}

class Solution {
    public int orangesRotting(int[][] grid) {
        int count=0;
        Deque<int[]> q=new ArrayDeque<>();
        final int[][] RLDU={{1,0},{0,1},{-1,0},{0,-1}};
        int r,c;
        for(int i=0;i<grid.length;i++)
        {
            for(int j=0;j<grid[0].length;j++)
            {
                if(grid[i][j]==2)
                    q.offer(new int[]{i,j});
            }
        }
        while(!q.isEmpty())
        {
            int size=q.size();
            for(int i=0;i<size;i++)
            {
                r=q.peek()[0];
                c=q.poll()[1];
                for(int[] dir : RLDU)
                {
                    if(r+dir[0]<0 || c+dir[1]<0 || r+dir[0]>grid.length-1 || c+dir[1]>grid[0].length-1 || grid[r+dir[0]][c+dir[1]]==0 || grid[r+dir[0]][c+dir[1]]==2) continue;
                    grid[r+dir[0]][c+dir[1]]=0;
                    q.offer(new int[]{r+dir[0],c+dir[1]});
                }
            }
            count++;
        }
        for(int i=0;i<grid.length;i++)
        {
            for(int j=0;j<grid[0].length;j++)
            {
                if(grid[i][j]==1)
                    return -1;
            }
        }
        return Math.max(count-1,0);
    }
}

class Solution {
    public int[][] kClosest(int[][] points, int k) {
        PriorityQueue<int[]> pq = new PriorityQueue<int[]>((a,b)->(b[0]*b[0] + b[1]*b[1]) - (a[0]*a[0] + a[1]*a[1]));
        int[][] res;
        for(int[] xy: points)
        {
            pq.offer(xy);
            if(pq.size()>k)
                pq.poll();
        }
        res=new int[pq.size()][2];
        int i=0;
        for(int[] num:pq)
        {
            res[i++]=num;
        }
        return res;
    }
}

