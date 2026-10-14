import { createContext, useContext } from "react";
import { projectApi, type ProjectApi } from "./api";

export const API_BASE = "";

export const api: ProjectApi = projectApi(API_BASE);

export const ApiContext = createContext<ProjectApi>(api);
export const useApi = () => useContext(ApiContext);
