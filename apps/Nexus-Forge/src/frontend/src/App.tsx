import { BrowserRouter, Route, Routes } from "react-router-dom";
import TopNav from "./components/TopNav";
import AISafetyOpsHub from "./pages/AISafetyOpsHub";
import AdaptiveComplianceOrchestratorHub from "./pages/AdaptiveComplianceOrchestratorHub";
import AdminConsoleHub from "./pages/AdminConsoleHub";
import AiCapabilityInventoryHub from "./pages/AiCapabilityInventoryHub";
import ApiEvolutionControlTowerHub from "./pages/ApiEvolutionControlTowerHub";
import ApiHub from "./pages/ApiHub";
import ArchitectureDecisionHub from "./pages/ArchitectureDecisionHub";
import ArtifactTrustChainHub from "./pages/ArtifactTrustChainHub";
import AuditIntelligenceHub from "./pages/AuditIntelligenceHub";
import AutomationHub from "./pages/AutomationHub";
import AutonomousBacklogTriageHub from "./pages/AutonomousBacklogTriageHub";
import AutonomousReleaseGovernorHub from "./pages/AutonomousReleaseGovernorHub";
import BillingCoreHub from "./pages/BillingCoreHub";
import BranchProtectionPoliciesHub from "./pages/BranchProtectionPoliciesHub";
import BrandConsistencyGuardHub from "./pages/BrandConsistencyGuardHub";
import BudgetAllocationEngineHub from "./pages/BudgetAllocationEngineHub";
import CapabilityMaturityRadarHub from "./pages/CapabilityMaturityRadarHub";
import CarbonAwareRuntimeHub from "./pages/CarbonAwareRuntimeHub";
import ChangeManagementOfficeHub from "./pages/ChangeManagementOfficeHub";
import ChatOpsHub from "./pages/ChatOpsHub";
import CiCdFullDepthHub from "./pages/CiCdFullDepthHub";
import CloudCostAnomalyLabHub from "./pages/CloudCostAnomalyLabHub";
import CodeOwnershipTrackerHub from "./pages/CodeOwnershipTrackerHub";
import CollaborationHub from "./pages/CollaborationHub";
import CommitSigningVaultHub from "./pages/CommitSigningVaultHub";
import CommunicationHub from "./pages/CommunicationHub";
import ContentStudioHub from "./pages/ContentStudioHub";
import ContractTestingExchangeHub from "./pages/ContractTestingExchangeHub";
import CrossCloudOrchestrationHub from "./pages/CrossCloudOrchestrationHub";
import CrossProductDependencyIntelligenceHub from "./pages/CrossProductDependencyIntelligenceHub";
import CrossTeamDependencyGraphHub from "./pages/CrossTeamDependencyGraphHub";
import CrossVcsRepositorySyncHub from "./pages/CrossVcsRepositorySyncHub";
import CustomerDataPrivacyVaultHub from "./pages/CustomerDataPrivacyVaultHub";
import CustomerJourneyOrchestrationHub from "./pages/CustomerJourneyOrchestrationHub";
import CustomerLifetimeVaultHub from "./pages/CustomerLifetimeVaultHub";
import CustomerSuccessHub from "./pages/CustomerSuccessHub";
import CustomerValueObservatoryHub from "./pages/CustomerValueObservatoryHub";
import DataContractRegistryHub from "./pages/DataContractRegistryHub";
import DataGovernanceFabricHub from "./pages/DataGovernanceFabricHub";
import DemandForecastingGridHub from "./pages/DemandForecastingGridHub";
import DependencyRemediationOpsHub from "./pages/DependencyRemediationOpsHub";
import DesignAssetManagementHub from "./pages/DesignAssetManagementHub";
import DevExHub from "./pages/DevExHub";
import DeveloperCredentialGraphHub from "./pages/DeveloperCredentialGraphHub";
import DeveloperJourneyIntelligenceHub from "./pages/DeveloperJourneyIntelligenceHub";
import DigitalEthicsGovernanceHub from "./pages/DigitalEthicsGovernanceHub";
import DigitalTwinHub from "./pages/DigitalTwinHub";
import DisasterRecoveryHub from "./pages/DisasterRecoveryHub";
import DomainWorkbench from "./pages/DomainWorkbench";
import EcosystemComplianceExchangeHub from "./pages/EcosystemComplianceExchangeHub";
import EdgeHub from "./pages/EdgeHub";
import EngineeringCognitionWorkspaceHub from "./pages/EngineeringCognitionWorkspaceHub";
import EnterpriseHub from "./pages/EnterpriseHub";
import EnterpriseKnowledgeGraphHub from "./pages/EnterpriseKnowledgeGraphHub";
import EnterpriseMigrationFactoryHub from "./pages/EnterpriseMigrationFactoryHub";
import EnterpriseSupportCommandHub from "./pages/EnterpriseSupportCommandHub";
import EventHub from "./pages/EventHub";
import ExecutiveCommandCenterHub from "./pages/ExecutiveCommandCenterHub";
import ExperimentationPlatformHub from "./pages/ExperimentationPlatformHub";
import FeatureCatalog from "./pages/FeatureCatalog";
import FeatureGatingOrchestratorHub from "./pages/FeatureGatingOrchestratorHub";
import FederatedIdentityAssuranceHub from "./pages/FederatedIdentityAssuranceHub";
import FinOpsHub from "./pages/FinOpsHub";
import FutureLabHub from "./pages/FutureLabHub";
import GlobalizationReadinessHub from "./pages/GlobalizationReadinessHub";
import GovernanceRiskCouncilHub from "./pages/GovernanceRiskCouncilHub";
import HybridWorkforceRouterHub from "./pages/HybridWorkforceRouterHub";
import IdentityHub from "./pages/IdentityHub";
import IncidentLearningSystemHub from "./pages/IncidentLearningSystemHub";
import IncidentPreventionGraphHub from "./pages/IncidentPreventionGraphHub";
import InfrastructureOptimizationLabHub from "./pages/InfrastructureOptimizationLabHub";
import InfrastructurePortfolioHub from "./pages/InfrastructurePortfolioHub";
import IntegrationExchangeHub from "./pages/IntegrationExchangeHub";
import InteropHub from "./pages/InteropHub";
import InventoryStubEngineHub from "./pages/InventoryStubEngineHub";
import IssueManagementFullDepthHub from "./pages/IssueManagementFullDepthHub";
import KnowledgeHub from "./pages/KnowledgeHub";
import LearningCenterHub from "./pages/LearningCenterHub";
import LegalOpsHub from "./pages/LegalOpsHub";
import LegislativeChangeRadarHub from "./pages/LegislativeChangeRadarHub";
import LicenseComplianceTrackerHub from "./pages/LicenseComplianceTrackerHub";
import LocalizationHub from "./pages/LocalizationHub";
import MLOpsHub from "./pages/MLOpsHub";
import MachineTranslationPlatformHub from "./pages/MachineTranslationPlatformHub";
import MergeStrategyEngineHub from "./pages/MergeStrategyEngineHub";
import ModelMarketplaceOpsHub from "./pages/ModelMarketplaceOpsHub";
import MultiCloudFailoverMeshHub from "./pages/MultiCloudFailoverMeshHub";
import MultiTenantDataIsolationHub from "./pages/MultiTenantDataIsolationHub";
import NotebookHub from "./pages/NotebookHub";
import NotificationOrchestrationHub from "./pages/NotificationOrchestrationHub";
import ObservabilityHub from "./pages/ObservabilityHub";
import OperationalNarrativeStudioHub from "./pages/OperationalNarrativeStudioHub";
import OperationsHub from "./pages/OperationsHub";
import OrgDesignStudioHub from "./pages/OrgDesignStudioHub";
import PackageRegistryMatrixHub from "./pages/PackageRegistryMatrixHub";
import PagesHostingHub from "./pages/PagesHostingHub";
import PartnerEcosystemHub from "./pages/PartnerEcosystemHub";
import PerformanceTestingLabHub from "./pages/PerformanceTestingLabHub";
import PlatformEconomicsHub from "./pages/PlatformEconomicsHub";
import PlatformGoalCascadeHub from "./pages/PlatformGoalCascadeHub";
import PlatformHub from "./pages/PlatformHub";
import PlatformTrustAutomationHub from "./pages/PlatformTrustAutomationHub";
import PluginSandboxHub from "./pages/PluginSandboxHub";
import PolicyAsCodeStudioHub from "./pages/PolicyAsCodeStudioHub";
import PolicyConflictResolverHub from "./pages/PolicyConflictResolverHub";
import PolicyLabHub from "./pages/PolicyLabHub";
import PortfolioSynergyEngineHub from "./pages/PortfolioSynergyEngineHub";
import ProcurementHub from "./pages/ProcurementHub";
import ProductAnalyticsIntelligenceHub from "./pages/ProductAnalyticsIntelligenceHub";
import ProductExperimentLedgerHub from "./pages/ProductExperimentLedgerHub";
import ProductTelemetryIntelHub from "./pages/ProductTelemetryIntelHub";
import ProjectBoardsFullDepthHub from "./pages/ProjectBoardsFullDepthHub";
import PullRequest from "./pages/PullRequest";
import QualityHub from "./pages/QualityHub";
import QuantumResilienceLabHub from "./pages/QuantumResilienceLabHub";
import ReleaseCadenceOptimizerHub from "./pages/ReleaseCadenceOptimizerHub";
import ReleaseEngineeringHub from "./pages/ReleaseEngineeringHub";
import ReleaseReliabilityScorecardsHub from "./pages/ReleaseReliabilityScorecardsHub";
import RepoDetail from "./pages/RepoDetail";
import RepoList from "./pages/RepoList";
import RepositoryMirrorFederationHub from "./pages/RepositoryMirrorFederationHub";
import RequirementsManagementHub from "./pages/RequirementsManagementHub";
import ResearchLabOpsHub from "./pages/ResearchLabOpsHub";
import ResilienceHub from "./pages/ResilienceHub";
import RevenueHub from "./pages/RevenueHub";
import RoadmapDependencySimulatorHub from "./pages/RoadmapDependencySimulatorHub";
import RuntimeGovernanceHub from "./pages/RuntimeGovernanceHub";
import SecretsVaultHub from "./pages/SecretsVaultHub";
import SecurityChaosEngineeringHub from "./pages/SecurityChaosEngineeringHub";
import SemanticSearchIndexHub from "./pages/SemanticSearchIndexHub";
import ServiceCatalogHub from "./pages/ServiceCatalogHub";
import ServiceLevelEconomicsHub from "./pages/ServiceLevelEconomicsHub";
import SnippetsGistsHub from "./pages/SnippetsGistsHub";
import SovereignCloudOpsHub from "./pages/SovereignCloudOpsHub";
import StakeholderCommunicationIntelligenceHub from "./pages/StakeholderCommunicationIntelligenceHub";
import StrategicCapacityCommandHub from "./pages/StrategicCapacityCommandHub";
import StrategyOKRHub from "./pages/StrategyOKRHub";
import SupplyChainHub from "./pages/SupplyChainHub";
import SupplyChainVisibilityHub from "./pages/SupplyChainVisibilityHub";
import SupplyContinuityPlannerHub from "./pages/SupplyContinuityPlannerHub";
import SustainabilityCommandCenterHub from "./pages/SustainabilityCommandCenterHub";
import TeamManage from "./pages/TeamManage";
import TechnicalDebtKeeperHub from "./pages/TechnicalDebtKeeperHub";
import TenancyHub from "./pages/TenancyHub";
import TestQualityManagementHub from "./pages/TestQualityManagementHub";
import TrustAnalyticsHub from "./pages/TrustAnalyticsHub";
import TrustCenterHub from "./pages/TrustCenterHub";
import VendorRiskOrchestrationHub from "./pages/VendorRiskOrchestrationHub";
import WorkflowMarketplaceHub from "./pages/WorkflowMarketplaceHub";
import WorkforceSkillMarketplaceHub from "./pages/WorkforceSkillMarketplaceHub";
import WorkloadOrchestrationEngineHub from "./pages/WorkloadOrchestrationEngineHub";

export default function App() {
  return (
    <BrowserRouter>
      <div className="app-shell">
        <TopNav />
        <main className="app-content">
          <Routes>
            <Route path="/" element={<RepoList />} />
            <Route path="/repos/:name" element={<RepoDetail />} />
            <Route path="/pull-requests" element={<PullRequest />} />
            <Route path="/teams" element={<TeamManage />} />
            <Route path="/features" element={<FeatureCatalog />} />
            <Route path="/platform" element={<PlatformHub />} />
            <Route path="/platform/:domain" element={<DomainWorkbench />} />
            <Route path="/collaboration" element={<CollaborationHub />} />
            <Route path="/knowledge" element={<KnowledgeHub />} />
            <Route path="/automation" element={<AutomationHub />} />
            <Route path="/enterprise" element={<EnterpriseHub />} />
            <Route path="/labs" element={<FutureLabHub />} />
            <Route path="/operations" element={<OperationsHub />} />
            <Route path="/revenue" element={<RevenueHub />} />
            <Route path="/interop" element={<InteropHub />} />
            <Route path="/identity" element={<IdentityHub />} />
            <Route path="/edge" element={<EdgeHub />} />
            <Route path="/mlops" element={<MLOpsHub />} />
            <Route path="/quality" element={<QualityHub />} />
            <Route path="/supply-chain" element={<SupplyChainHub />} />
            <Route path="/api-hub" element={<ApiHub />} />
            <Route path="/chatops" element={<ChatOpsHub />} />
            <Route path="/notebooks" element={<NotebookHub />} />
            <Route path="/localization" element={<LocalizationHub />} />
            <Route path="/resilience" element={<ResilienceHub />} />
            <Route path="/events" element={<EventHub />} />
            <Route path="/admin-console" element={<AdminConsoleHub />} />
            <Route path="/service-catalog" element={<ServiceCatalogHub />} />
            <Route path="/finops" element={<FinOpsHub />} />
            <Route path="/policy-lab" element={<PolicyLabHub />} />
            <Route path="/release-engineering" element={<ReleaseEngineeringHub />} />
            <Route path="/trust-center" element={<TrustCenterHub />} />
            <Route path="/communications" element={<CommunicationHub />} />
            <Route path="/devex" element={<DevExHub />} />
            <Route path="/legal-ops" element={<LegalOpsHub />} />
            <Route path="/secrets-vault" element={<SecretsVaultHub />} />
            <Route path="/observability" element={<ObservabilityHub />} />
            <Route path="/procurement" element={<ProcurementHub />} />
            <Route path="/customer-success" element={<CustomerSuccessHub />} />
            <Route path="/learning-center" element={<LearningCenterHub />} />
            <Route path="/tenancy" element={<TenancyHub />} />
            <Route path="/billing-core" element={<BillingCoreHub />} />
            <Route path="/content-studio" element={<ContentStudioHub />} />
            <Route path="/integration-exchange" element={<IntegrationExchangeHub />} />
            <Route path="/runtime-governance" element={<RuntimeGovernanceHub />} />
            <Route path="/audit-intelligence" element={<AuditIntelligenceHub />} />
            <Route path="/plugin-sandbox" element={<PluginSandboxHub />} />
            <Route path="/data-governance-fabric" element={<DataGovernanceFabricHub />} />
            <Route path="/strategy-okr" element={<StrategyOKRHub />} />
            <Route path="/ai-safety-ops" element={<AISafetyOpsHub />} />
            <Route path="/partner-ecosystem" element={<PartnerEcosystemHub />} />
            <Route path="/disaster-recovery" element={<DisasterRecoveryHub />} />
            <Route path="/workflow-marketplace" element={<WorkflowMarketplaceHub />} />
            <Route path="/digital-twin" element={<DigitalTwinHub />} />
            <Route path="/trust-analytics" element={<TrustAnalyticsHub />} />
            <Route path="/research-lab-ops" element={<ResearchLabOpsHub />} />
            <Route path="/experimentation-platform" element={<ExperimentationPlatformHub />} />
            <Route path="/platform-economics" element={<PlatformEconomicsHub />} />
            <Route path="/change-management-office" element={<ChangeManagementOfficeHub />} />
            <Route path="/executive-command-center" element={<ExecutiveCommandCenterHub />} />
            <Route path="/product-telemetry-intel" element={<ProductTelemetryIntelHub />} />
            <Route
              path="/customer-journey-orchestration"
              element={<CustomerJourneyOrchestrationHub />}
            />
            <Route path="/architecture-decision-hub" element={<ArchitectureDecisionHub />} />
            <Route path="/model-marketplace-ops" element={<ModelMarketplaceOpsHub />} />
            <Route path="/infrastructure-portfolio" element={<InfrastructurePortfolioHub />} />
            <Route path="/governance-risk-council" element={<GovernanceRiskCouncilHub />} />
            <Route
              path="/release-reliability-scorecards"
              element={<ReleaseReliabilityScorecardsHub />}
            />
            <Route
              path="/enterprise-migration-factory"
              element={<EnterpriseMigrationFactoryHub />}
            />
            <Route path="/autonomous-backlog-triage" element={<AutonomousBacklogTriageHub />} />
            <Route
              path="/cross-product-dependency-intelligence"
              element={<CrossProductDependencyIntelligenceHub />}
            />
            <Route path="/demand-forecasting-grid" element={<DemandForecastingGridHub />} />
            <Route path="/policy-as-code-studio" element={<PolicyAsCodeStudioHub />} />
            <Route path="/enterprise-support-command" element={<EnterpriseSupportCommandHub />} />
            <Route path="/data-contract-registry" element={<DataContractRegistryHub />} />
            <Route path="/capability-maturity-radar" element={<CapabilityMaturityRadarHub />} />
            <Route path="/portfolio-synergy-engine" element={<PortfolioSynergyEngineHub />} />
            <Route path="/sovereign-cloud-ops" element={<SovereignCloudOpsHub />} />
            <Route path="/incident-learning-system" element={<IncidentLearningSystemHub />} />
            <Route path="/contract-testing-exchange" element={<ContractTestingExchangeHub />} />
            <Route path="/org-design-studio" element={<OrgDesignStudioHub />} />
            <Route path="/platform-trust-automation" element={<PlatformTrustAutomationHub />} />
            <Route path="/strategic-capacity-command" element={<StrategicCapacityCommandHub />} />
            <Route
              path="/ecosystem-compliance-exchange"
              element={<EcosystemComplianceExchangeHub />}
            />
            <Route
              path="/roadmap-dependency-simulator"
              element={<RoadmapDependencySimulatorHub />}
            />
            <Route
              path="/engineering-cognition-workspace"
              element={<EngineeringCognitionWorkspaceHub />}
            />
            <Route
              path="/sustainability-command-center"
              element={<SustainabilityCommandCenterHub />}
            />
            <Route path="/api-evolution-control-tower" element={<ApiEvolutionControlTowerHub />} />
            <Route
              path="/developer-journey-intelligence"
              element={<DeveloperJourneyIntelligenceHub />}
            />
            <Route path="/artifact-trust-chain" element={<ArtifactTrustChainHub />} />
            <Route path="/enterprise-knowledge-graph" element={<EnterpriseKnowledgeGraphHub />} />
            <Route path="/quantum-resilience-lab" element={<QuantumResilienceLabHub />} />
            <Route path="/vendor-risk-orchestration" element={<VendorRiskOrchestrationHub />} />
            <Route path="/service-level-economics" element={<ServiceLevelEconomicsHub />} />
            <Route path="/developer-credential-graph" element={<DeveloperCredentialGraphHub />} />
            <Route path="/autonomous-release-governor" element={<AutonomousReleaseGovernorHub />} />
            <Route
              path="/adaptive-compliance-orchestrator"
              element={<AdaptiveComplianceOrchestratorHub />}
            />
            <Route path="/incident-prevention-graph" element={<IncidentPreventionGraphHub />} />
            <Route path="/cloud-cost-anomaly-lab" element={<CloudCostAnomalyLabHub />} />
            <Route path="/release-cadence-optimizer" element={<ReleaseCadenceOptimizerHub />} />
            <Route path="/workforce-skill-marketplace" element={<WorkforceSkillMarketplaceHub />} />
            <Route path="/customer-value-observatory" element={<CustomerValueObservatoryHub />} />
            <Route path="/security-chaos-engineering" element={<SecurityChaosEngineeringHub />} />
            <Route path="/legislative-change-radar" element={<LegislativeChangeRadarHub />} />
            <Route
              path="/federated-identity-assurance"
              element={<FederatedIdentityAssuranceHub />}
            />
            <Route path="/platform-goal-cascade" element={<PlatformGoalCascadeHub />} />
            <Route path="/digital-ethics-governance" element={<DigitalEthicsGovernanceHub />} />
            <Route path="/supply-continuity-planner" element={<SupplyContinuityPlannerHub />} />
            <Route path="/multi-cloud-failover-mesh" element={<MultiCloudFailoverMeshHub />} />
            <Route path="/product-experiment-ledger" element={<ProductExperimentLedgerHub />} />
            <Route
              path="/stakeholder-communication-intelligence"
              element={<StakeholderCommunicationIntelligenceHub />}
            />
            <Route path="/carbon-aware-runtime" element={<CarbonAwareRuntimeHub />} />
            <Route path="/policy-conflict-resolver" element={<PolicyConflictResolverHub />} />
            <Route path="/globalization-readiness" element={<GlobalizationReadinessHub />} />
            <Route path="/dependency-remediation-ops" element={<DependencyRemediationOpsHub />} />
            <Route
              path="/operational-narrative-studio"
              element={<OperationalNarrativeStudioHub />}
            />
            <Route path="/ai-capability-inventory" element={<AiCapabilityInventoryHub />} />
            <Route path="/supply-chain-visibility" element={<SupplyChainVisibilityHub />} />
            <Route path="/hybrid-workforce-router" element={<HybridWorkforceRouterHub />} />
            <Route path="/customer-lifetime-vault" element={<CustomerLifetimeVaultHub />} />
            <Route path="/technical-debt-keeper" element={<TechnicalDebtKeeperHub />} />
            <Route path="/multi-tenant-data-isolation" element={<MultiTenantDataIsolationHub />} />
            <Route
              path="/product-analytics-intelligence"
              element={<ProductAnalyticsIntelligenceHub />}
            />
            <Route
              path="/infrastructure-optimization-lab"
              element={<InfrastructureOptimizationLabHub />}
            />
            <Route path="/license-compliance-tracker" element={<LicenseComplianceTrackerHub />} />
            <Route path="/cross-team-dependency-graph" element={<CrossTeamDependencyGraphHub />} />
            <Route path="/feature-gating-orchestrator" element={<FeatureGatingOrchestratorHub />} />
            <Route path="/customer-data-privacy-vault" element={<CustomerDataPrivacyVaultHub />} />
            <Route
              path="/workload-orchestration-engine"
              element={<WorkloadOrchestrationEngineHub />}
            />
            <Route path="/brand-consistency-guard" element={<BrandConsistencyGuardHub />} />
            <Route path="/performance-testing-lab" element={<PerformanceTestingLabHub />} />
            <Route path="/semantic-search-index" element={<SemanticSearchIndexHub />} />
            <Route path="/notification-orchestration" element={<NotificationOrchestrationHub />} />
            <Route path="/budget-allocation-engine" element={<BudgetAllocationEngineHub />} />
            <Route path="/cross-cloud-orchestration" element={<CrossCloudOrchestrationHub />} />
            <Route
              path="/machine-translation-platform"
              element={<MachineTranslationPlatformHub />}
            />
            <Route path="/merge-strategy-engine" element={<MergeStrategyEngineHub />} />
            <Route path="/branch-protection-policies" element={<BranchProtectionPoliciesHub />} />
            <Route path="/commit-signing-vault" element={<CommitSigningVaultHub />} />
            <Route path="/code-ownership-tracker" element={<CodeOwnershipTrackerHub />} />
            <Route path="/cross-vcs-repository-sync" element={<CrossVcsRepositorySyncHub />} />
            <Route path="/inventory-stub-engine" element={<InventoryStubEngineHub />} />
            <Route path="/ci-cd-full-depth" element={<CiCdFullDepthHub />} />
            <Route path="/package-registry-matrix" element={<PackageRegistryMatrixHub />} />
            <Route
              path="/repository-mirror-federation"
              element={<RepositoryMirrorFederationHub />}
            />
            <Route path="/snippets-gists" element={<SnippetsGistsHub />} />
            <Route path="/pages-hosting" element={<PagesHostingHub />} />
            <Route path="/issue-management-full-depth" element={<IssueManagementFullDepthHub />} />
            <Route path="/project-boards-full-depth" element={<ProjectBoardsFullDepthHub />} />
            <Route path="/test-quality-management" element={<TestQualityManagementHub />} />
            <Route path="/requirements-management" element={<RequirementsManagementHub />} />
            <Route path="/design-asset-management" element={<DesignAssetManagementHub />} />
          </Routes>
        </main>
      </div>
    </BrowserRouter>
  );
}
