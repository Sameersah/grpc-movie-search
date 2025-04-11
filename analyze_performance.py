#!/usr/bin/env python3
# analyze_performance.py

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import glob
import os

# Create results directory for charts
os.makedirs("performance_charts", exist_ok=True)

# Load all result files
result_files = glob.glob("performance_results/*.csv")
all_data = []

for file in result_files:
    config_name = os.path.basename(file).replace('_results.csv', '')
    df = pd.read_csv(file)
    df['Configuration'] = config_name

    # Extract node type
    if 'single_node' in config_name:
        df['NodeType'] = 'Single Node'
    elif 'multi_node' in config_name:
        df['NodeType'] = 'Multi Node'
    else:
        df['NodeType'] = 'Unknown'

    # Extract communication type
    if 'grpc' in config_name:
        df['Communication'] = 'gRPC'
    elif 'shm' in config_name:
        df['Communication'] = 'Shared Memory'
    else:
        df['Communication'] = 'Unknown'

    # Extract cache setting
    if 'no_cache' in config_name:
        df['Cache'] = 'Disabled'
    else:
        df['Cache'] = 'Enabled'

    all_data.append(df)

# Combine all data
combined_df = pd.concat(all_data)

# Calculate average duration by configuration and run type
avg_duration = combined_df.groupby(['Configuration', 'NodeType', 'Communication', 'Cache', 'RunType'])['Duration(ms)'].mean().reset_index()

# Plot 1: Single vs Multi Node Performance (Warm runs)
plt.figure(figsize=(14, 8))
node_type_df = avg_duration[(avg_duration['RunType'] == 'warm') &
                            (avg_duration['Cache'] == 'Enabled')]

if len(node_type_df[node_type_df['NodeType'] == 'Multi Node']) > 0:
    sns.barplot(x='NodeType', y='Duration(ms)', hue='Communication', data=node_type_df)
    plt.title('Performance Comparison: Single Node vs. Multi Node', fontsize=16)
    plt.ylabel('Average Response Time (ms)', fontsize=14)
    plt.xlabel('Deployment Type', fontsize=14)
    plt.grid(axis='y', linestyle='--', alpha=0.7)
    plt.tight_layout()
    plt.savefig('performance_charts/node_type_comparison.png')

# Plot 2: Communication Method Comparison (Single Node)
plt.figure(figsize=(14, 8))
single_node_df = avg_duration[(avg_duration['NodeType'] == 'Single Node') &
                              (avg_duration['RunType'] == 'warm')]
sns.barplot(x='Communication', y='Duration(ms)', hue='Cache', data=single_node_df)
plt.title('Single Node: gRPC vs. Shared Memory Performance', fontsize=16)
plt.ylabel('Average Response Time (ms)', fontsize=14)
plt.xlabel('Communication Method', fontsize=14)
plt.grid(axis='y', linestyle='--', alpha=0.7)
plt.tight_layout()
plt.savefig('performance_charts/single_node_communication_comparison.png')

# Plot 3: Cache Impact Across Deployments
plt.figure(figsize=(14, 8))
cache_df = avg_duration[avg_duration['RunType'] == 'warm']
sns.barplot(x='Cache', y='Duration(ms)', hue='NodeType', data=cache_df)
plt.title('Cache Impact Across Deployment Types', fontsize=16)
plt.ylabel('Average Response Time (ms)', fontsize=14)
plt.xlabel('Cache Setting', fontsize=14)
plt.grid(axis='y', linestyle='--', alpha=0.7)
plt.tight_layout()
plt.savefig('performance_charts/cache_impact_by_node_type.png')

# Plot 4: Cold vs Warm Performance
plt.figure(figsize=(15, 8))
config_order = ['single_node_shm_cache', 'single_node_grpc_cache',
                'single_node_shm_no_cache', 'single_node_grpc_no_cache']
if 'multi_node_grpc_cache' in avg_duration['Configuration'].values:
    config_order.extend(['multi_node_grpc_cache', 'multi_node_grpc_no_cache'])

# Filter and sort by the defined order
plot_df = avg_duration[avg_duration['Configuration'].isin(config_order)].copy()
plot_df['Configuration'] = pd.Categorical(plot_df['Configuration'], categories=config_order, ordered=True)
plot_df = plot_df.sort_values('Configuration')

sns.barplot(x='Configuration', y='Duration(ms)', hue='RunType', data=plot_df)
plt.title('Cold vs. Warm Performance by Configuration', fontsize=16)
plt.ylabel('Average Response Time (ms)', fontsize=14)
plt.xlabel('Configuration', fontsize=14)
plt.xticks(rotation=45)
plt.grid(axis='y', linestyle='--', alpha=0.7)
plt.tight_layout()
plt.savefig('performance_charts/cold_vs_warm.png')

# Plot 5: Query-specific performance
query_perf = combined_df[(combined_df['RunType'] == 'warm') &
                         (combined_df['NodeType'] == 'Single Node')].groupby(['Query', 'Communication'])['Duration(ms)'].mean().reset_index()
plt.figure(figsize=(14, 8))
sns.barplot(x='Query', y='Duration(ms)', hue='Communication', data=query_perf)
plt.title('Query Performance by Communication Method (Single Node)', fontsize=16)
plt.ylabel('Average Response Time (ms)', fontsize=14)
plt.xlabel('Query', fontsize=14)
plt.xticks(rotation=45)
plt.grid(axis='y', linestyle='--', alpha=0.7)
plt.tight_layout()
plt.savefig('performance_charts/query_performance.png')

# Summary statistics
summary = combined_df.groupby(['Configuration', 'NodeType', 'Communication', 'Cache', 'RunType'])['Duration(ms)'].agg(['mean', 'min', 'max', 'count']).reset_index()
summary.to_csv('performance_charts/summary_statistics.csv', index=False)

# Create detailed performance report
with open('performance_charts/performance_report.txt', 'w') as f:
    f.write("# Performance Analysis Report\n\n")

    # Overall comparisons
    warm_stats = summary[summary['RunType'] == 'warm']

    f.write("## Overall Performance (Warm Runs)\n\n")
    f.write("| Configuration | Node Type | Communication | Cache | Avg Response (ms) | Min | Max | Count |\n")
    f.write("|---------------|-----------|---------------|-------|-------------------|-----|-----|-------|\n")

    for _, row in warm_stats.iterrows():
        f.write(f"| {row['Configuration']} | {row['NodeType']} | {row['Communication']} | {row['Cache']} | {row['mean']:.2f} | {row['min']:.2f} | {row['max']:.2f} | {row['count']} |\n")

    # Single node: Communication method comparison
    single_node_data = warm_stats[warm_stats['NodeType'] == 'Single Node']
    if not single_node_data.empty:
        f.write("\n## Single Node: Communication Method Comparison\n\n")
        for cache in ['Enabled', 'Disabled']:
            cache_data = single_node_data[single_node_data['Cache'] == cache]
            if not cache_data.empty:
                f.write(f"\nWith Cache {cache}:\n")
                grpc_data = cache_data[cache_data['Communication'] == 'gRPC']
                shm_data = cache_data[cache_data['Communication'] == 'Shared Memory']

                if not grpc_data.empty and not shm_data.empty:
                    grpc_time = grpc_data['mean'].values[0]
                    shm_time = shm_data['mean'].values[0]
                    improvement = ((grpc_time - shm_time) / grpc_time) * 100

                    f.write(f"- gRPC: {grpc_time:.2f} ms\n")
                    f.write(f"- Shared Memory: {shm_time:.2f} ms\n")
                    f.write(f"- Improvement: {improvement:.2f}%\n")

    # Multi-node analysis
    multi_node_data = warm_stats[warm_stats['NodeType'] == 'Multi Node']
    if not multi_node_data.empty:
        f.write("\n## Multi-Node Performance\n\n")
        for cache in ['Enabled', 'Disabled']:
            cache_data = multi_node_data[multi_node_data['Cache'] == cache]
            if not cache_data.empty:
                f.write(f"\nWith Cache {cache}:\n")
                for _, row in cache_data.iterrows():
                    f.write(f"- {row['Communication']}: {row['mean']:.2f} ms\n")

        # Compare to single node
        comparable_single = single_node_data[single_node_data['Communication'] == 'gRPC']
        comparable_multi = multi_node_data[multi_node_data['Communication'] == 'gRPC']

        if not comparable_single.empty and not comparable_multi.empty:
            f.write("\n## Single Node vs. Multi-Node Comparison (gRPC only)\n\n")
            for cache in ['Enabled', 'Disabled']:
                single_cache = comparable_single[comparable_single['Cache'] == cache]
                multi_cache = comparable_multi[comparable_multi['Cache'] == cache]

                if not single_cache.empty and not multi_cache.empty:
                    single_time = single_cache['mean'].values[0]
                    multi_time = multi_cache['mean'].values[0]
                    difference = ((multi_time - single_time) / single_time) * 100

                    f.write(f"\nWith Cache {cache}:\n")
                    f.write(f"- Single Node: {single_time:.2f} ms\n")
                    f.write(f"- Multi Node: {multi_time:.2f} ms\n")
                    f.write(f"- Difference: {difference:.2f}% {'slower' if difference > 0 else 'faster'}\n")

print("Analysis complete! Results saved to performance_charts/")